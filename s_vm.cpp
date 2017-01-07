#include <stdarg.h>

#include "s_object.h"
#include "s_memory.h"
#include "s_gc.h"
#include "s_vm.h"

#include "u_assert.h"
#include "u_file.h"
#include "u_misc.h"
#include "u_log.h"

#include "c_variable.h"

VAR(int, s_profile, "control profiling", 0, 1, 0);
VAR(u::string, s_profile_file, "profiling information file name", "profile.html");
VAR(float, s_profile_sample_size, "profile sample size in milliseconds", 0.01f, 1.0f, 0.1f);
VAR(int, s_stack_size, "VM stack size in MiB", 1, 32, 16);
VAR(int, s_cycle_stride, "instructions per VM cycle", 1, 512, 128);

namespace s {

void *VM::stackAllocateUninitialized(State *state, size_t size) {
    SharedState *shared = state->m_shared;
    if (U_UNLIKELY(shared->m_stackLength == 0)) {
        shared->m_stackLength = s_stack_size*1024*1024;
        shared->m_stackData = Memory::allocate(shared->m_stackLength);
    }
    const size_t newOffset = shared->m_stackOffset + size;
    if (U_UNLIKELY(newOffset > shared->m_stackLength)) {
        VM::error(state, "Stack overflow");
        return nullptr;
    }
    unsigned char *data = (unsigned char *)shared->m_stackData + shared->m_stackOffset;
    shared->m_stackOffset = newOffset;
    return data;
}

void *VM::stackAllocate(State *state, size_t size) {
    void *data = stackAllocateUninitialized(state, size);
    if (data) {
        memset(data, 0, size);
    }
    return data;
}

void VM::stackFree(State *state, void *data, size_t size) {
    SharedState *shared = state->m_shared;
    const size_t newOffset = shared->m_stackOffset - size;
    // free has to be done in reverse order so verify that
    U_ASSERT(data == (void *)((unsigned char *)shared->m_stackData + newOffset));
    shared->m_stackOffset = newOffset;
}

void VM::addFrame(State *state, size_t slots, size_t fastSlots) {
    auto *frame = (CallFrame *)stackAllocate(state, sizeof(CallFrame));
    if (!frame) return;
    frame->m_above = state->m_frame;
    frame->m_count = slots;
    frame->m_slots = (Object **)stackAllocate(state, sizeof(Object *) * slots);
    if (!frame->m_slots) {
        stackFree(state, frame, sizeof *frame);
        return;
    }
    frame->m_fastSlotsCount = fastSlots;
    // don't need to be initialized since fast slots are not subjected to
    // traditional garbage collection.
    frame->m_fastSlots = (Object ***)stackAllocateUninitialized(state, sizeof(Object **) * fastSlots);
    if (!frame->m_fastSlots) {
        stackFree(state, frame->m_slots, sizeof(Object *) * slots);
        stackFree(state, frame, sizeof *frame);
        return;
    }
    state->m_frame = frame;
}

void VM::delFrame(State *state) {
    CallFrame *frame = state->m_frame;
    CallFrame *above = frame->m_above;
    stackFree(state, frame->m_fastSlots, sizeof(Object **) * frame->m_fastSlotsCount);
    stackFree(state, frame->m_slots, sizeof(Object *) * frame->m_count);
    stackFree(state, frame, sizeof *frame);
    state->m_frame = above;
}

void VM::error(State *state, const char *fmt, ...) {
    U_ASSERT(state->m_runState == kRunning);
    va_list va;
    va_start(va, fmt);
    state->m_error = u::format(fmt, va);
    va_end(va);
    state->m_runState = kErrored;
}

void VM::printBacktrace(State *state) {
    for (State *current = state; current; current = current->m_parent) {
        int k = 1;
        for (CallFrame *frame = state->m_frame; frame; frame = frame->m_above, k++) {
            Instruction *instruction = frame->m_instructions;
            const char *file = nullptr;
            SourceRange line;
            int col;
            int row;
            bool found = SourceRecord::findSourcePosition(instruction->m_belongsTo->m_textFrom, &file, &line, &row, &col);
            U_ASSERT(found);
            u::Log::err("[script] => % 4d: \e[1m%s:%d:\e[0m %.*s\n", k, file, row+1, (int)(line.m_end - line.m_begin - 1), line.m_begin);
        }
    }
}

static constexpr long long k1MS = 1000000LL; // 1ms

long long VM::getClockDifference(struct timespec *targetClock,
                                 struct timespec *compareClock)
{
    struct timespec sentinel;
    if (!targetClock) targetClock = &sentinel;
    const int result = clock_gettime(CLOCK_MONOTONIC, targetClock);
    if (result != 0) { *(volatile int *)0 = 0; }; // TODO: better handling if monotonic is not supported?
    const long nsDifference = targetClock->tv_nsec - compareClock->tv_nsec;
    const int sDifference = targetClock->tv_sec - compareClock->tv_sec;
    return (long long)sDifference * 1000000000LL + (long long)nsDifference;
}

void VM::recordProfile(State *state) {
    if (!s_profile) return;

    struct timespec profileTime;
    long long nsDifference = getClockDifference(&profileTime, &state->m_shared->m_profileState.m_lastTime);
    if (nsDifference > (long long)(s_profile_sample_size * k1MS)) {
        state->m_shared->m_profileState.m_lastTime = profileTime;
        const int cycleCount = state->m_shared->m_cycleCount;
        Table *directTable = &state->m_shared->m_profileState.m_directTable;
        Table *indirectTable = &state->m_shared->m_profileState.m_indirectTable;
        int innerRange = 0;
        for (State *currentState = state; currentState; currentState = currentState->m_parent) {
            for (CallFrame *currentFrame = currentState->m_frame; currentFrame; currentFrame = currentFrame->m_above, ++innerRange) {
                Instruction *currentInstruction = currentFrame->m_instructions;
                const char *keyData = (char *)&currentInstruction->m_belongsTo;
                const size_t keyLength = sizeof currentInstruction->m_belongsTo;
                const size_t keyHash = djb2(keyData, keyLength);
                if (innerRange == 0) {
                    Field *free = nullptr;
                    Field *find = Table::lookupAllocWithHash(directTable, keyData, keyLength, keyHash, &free);
                    if (find)
                        find->m_value = (void *)((size_t)find->m_value + 1);
                    else
                        free->m_value = (void *)1;
                } else if (currentInstruction->m_belongsTo->m_lastCycleSeen != cycleCount) {
                    Field *free = nullptr;
                    Field *find = Table::lookupAllocWithHash(indirectTable, keyData, keyLength, keyHash, &free);
                    if (find)
                        find->m_value = (void *)((size_t)find->m_value + 1);
                    else
                        free->m_value = (void *)1;
                }
                currentInstruction->m_belongsTo->m_lastCycleSeen = cycleCount;
            }
            // TODO backoff profiling samples if it's slowing us down too much
            // and adding noise
        }
    }
}

#define VM_ASSERTION(CONDITION, ...) \
    do { \
        if (U_UNLIKELY(!(CONDITION)) && \
            (VM::error(state->m_restState, __VA_ARGS__), true)) \
        { \
            return { instrHalt }; \
        } \
    } while (0)

void VMState::refresh(VMState *state) {
    state->m_cf = state->m_restState->m_frame;
    state->m_instr = state->m_cf->m_instructions;
    state->m_slots = state->m_cf->m_slots;
}

static VMFnWrap instrNewObject(VMState *state) U_PURE;
static VMFnWrap instrNewIntObject(VMState *state) U_PURE;
static VMFnWrap instrNewFloatObject(VMState *state) U_PURE;
static VMFnWrap instrNewArrayObject(VMState *state) U_PURE;
static VMFnWrap instrNewStringObject(VMState *state) U_PURE;
static VMFnWrap instrNewClosureObject(VMState *state) U_PURE;
static VMFnWrap instrCloseObject(VMState *state) U_PURE;
static VMFnWrap instrSetConstraint(VMState *state) U_PURE;
static VMFnWrap instrAccess(VMState *state) U_PURE;
static VMFnWrap instrFreeze(VMState *state) U_PURE;
static VMFnWrap instrAccessStringKey(VMState *state) U_PURE;
static VMFnWrap instrAssign(VMState *state) U_PURE;
static VMFnWrap instrAssignStringKey(VMState *state) U_PURE;
static VMFnWrap instrSetConstraintStringKey(VMState *state) U_PURE;
static VMFnWrap instrCall(VMState *state) U_HOT;
static VMFnWrap instrSaveResult(VMState *state) U_PURE;
static VMFnWrap instrHalt(VMState *state) U_PURE;
static VMFnWrap instrReturn(VMState *state) U_PURE;
static VMFnWrap instrBranch(VMState *state) U_PURE;
static VMFnWrap instrTestBranch(VMState *state) U_HOT;
static VMFnWrap instrDefineFastSlot(VMState *state) U_PURE;
static VMFnWrap instrReadFastSlot(VMState *state) U_PURE;
static VMFnWrap instrWriteFastSlot(VMState *state) U_PURE;

static const VMInstrFn instrFunctions[] = {
    instrNewObject,
    instrNewIntObject,
    instrNewFloatObject,
    instrNewArrayObject,
    instrNewStringObject,
    instrNewClosureObject,
    instrCloseObject,
    instrSetConstraint,
    instrAccess,
    instrFreeze,
    instrAssign,
    instrCall,
    instrReturn,
    instrSaveResult,
    instrBranch,
    instrTestBranch,
    instrAccessStringKey,
    instrAssignStringKey,
    instrSetConstraintStringKey,
    instrDefineFastSlot,
    instrReadFastSlot,
    instrWriteFastSlot
};

static VMFnWrap instrNewObject(VMState *state) {
    const auto *instruction = (Instruction::NewObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    const Slot parentSlot = instruction->m_parentSlot;

    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(parentSlot < state->m_cf->m_count, "slot addressing error");

    Object *parentObject = state->m_slots[parentSlot];
    if (parentObject) {
        VM_ASSERTION(!(parentObject->m_flags & kNoInherit), "cannot inherit from this object");
    }
    state->m_slots[targetSlot] = Object::newObject(state->m_restState, parentObject);
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrNewIntObject(VMState *state) {
    auto *instruction = (Instruction::NewIntObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    const int value = instruction->m_value;
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    if (U_UNLIKELY(!instruction->m_intObject)) {
        Object *object = Object::newInt(state->m_restState, value);
        instruction->m_intObject = object;
        GC::addPermanent(state->m_restState, object);
    }
    state->m_slots[targetSlot] = instruction->m_intObject;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrNewFloatObject(VMState *state) {
    auto *instruction = (Instruction::NewFloatObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    const float value = instruction->m_value;
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    if (U_UNLIKELY(!instruction->m_floatObject)) {
        Object *object = Object::newFloat(state->m_restState, value);
        instruction->m_floatObject = object;
        GC::addPermanent(state->m_restState, object);
    }
    state->m_slots[targetSlot] = instruction->m_floatObject;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrNewArrayObject(VMState *state) {
    const auto *instruction = (Instruction::NewArrayObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    auto *array = Object::newArray(state->m_restState,
                                   nullptr,
                                   (IntObject *)state->m_restState->m_shared->m_valueCache.m_intZero);
    state->m_slots[targetSlot] = array;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrNewStringObject(VMState *state) {
    auto *instruction = (Instruction::NewStringObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    const char *value = instruction->m_value;
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    if (U_UNLIKELY(!instruction->m_stringObject)) {
        Object *object = Object::newString(state->m_restState, value, strlen(value));
        instruction->m_stringObject = object;
        GC::addPermanent(state->m_restState, object);
    }
    state->m_slots[targetSlot] = instruction->m_stringObject;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrNewClosureObject(VMState *state) {
    const auto *instruction = (Instruction::NewClosureObject *)state->m_instr;
    const Slot targetSlot = instruction->m_targetSlot;
    const Slot contextSlot = instruction->m_contextSlot;
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(contextSlot < state->m_cf->m_count, "slot addressing error");
    Object *context = state->m_slots[contextSlot];
    state->m_slots[targetSlot] = Object::newClosure(state->m_restState,
                                                    context,
                                                    instruction->m_function);
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrCloseObject(VMState *state) {
    const auto *instruction = (Instruction::CloseObject *)state->m_instr;
    const Slot slot = instruction->m_slot;
    VM_ASSERTION(slot < state->m_cf->m_count, "slot addressing error");
    Object *object = state->m_slots[slot];
    VM_ASSERTION(!(object->m_flags & kClosed), "object is already closed");
    object->m_flags |= kClosed;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrSetConstraint(VMState *state) {
    const auto *instruction = (Instruction::SetConstraint *)state->m_instr;
    const Slot keySlot = instruction->m_keySlot;
    const Slot objectSlot = instruction->m_objectSlot;
    const Slot constraintSlot = instruction->m_constraintSlot;
    VM_ASSERTION(keySlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(constraintSlot < state->m_cf->m_count, "slot addressing error");
    Object *object = state->m_slots[objectSlot];
    Object *constraint = state->m_slots[constraintSlot];
    Object *stringBase = state->m_restState->m_shared->m_valueCache.m_stringBase;
    Object *keyObject = state->m_slots[keySlot];
    StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
    VM_ASSERTION(stringKey, "internal error");
    const char *key = stringKey->m_value;
    const char *error = Object::setConstraint(state->m_restState, object, key, strlen(key), constraint);
    VM_ASSERTION(!error, "failed setting type constraint: %s", error);
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrAccess(VMState *state) {
    const auto *instruction = (Instruction::Access *)state->m_instr;

    const Slot objectSlot = instruction->m_objectSlot;
    const Slot targetSlot = instruction->m_targetSlot;

    const Slot keySlot = instruction->m_keySlot;

    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(keySlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(state->m_slots[keySlot], "null key slot");

    char *key = nullptr;
    bool hasKey = false;

    Object *object = state->m_slots[objectSlot];

    Object *stringBase = state->m_restState->m_shared->m_valueCache.m_stringBase;
    Object *keyObject = state->m_slots[keySlot];

    auto *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);

    bool objectFound = false;

    if (stringKey) {
        GC::addPermanent(state->m_restState, keyObject);
        key = stringKey->m_value;
        state->m_slots[targetSlot] = Object::lookup(object, key, &objectFound);
    }
    if (!objectFound) {
        Object *indexOperation = Object::lookup(object, "[]", nullptr);
        if (indexOperation) {
            Object *keyObject = state->m_slots[keySlot];

            State subState = { };
            subState.m_parent = state->m_restState;
            subState.m_root = state->m_root;
            subState.m_shared = state->m_restState->m_shared;

            if (!VM::callCallable(&subState, object, indexOperation, &keyObject, 1)) {
                return { instrHalt };
            }

            VM::run(&subState);

            VM_ASSERTION(subState.m_runState != kErrored, "[] overload failed: %s\n",
                subState.m_error.c_str());

            state->m_slots[targetSlot] = subState.m_resultValue;

            objectFound = true;
        }
    }
    if (!objectFound) {
        if (hasKey) {
            VM_ASSERTION(false, "property not found: '%s'", key);
        } else {
            VM_ASSERTION(false, "property not found");
        }
    }
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrFreeze(VMState *state) {
    const auto *instruction = (Instruction::Freeze *)state->m_instr;
    const Slot slot = instruction->m_slot;

    VM_ASSERTION(slot < state->m_cf->m_count, "slot addressing error");

    Object *object = state->m_slots[slot];
    VM_ASSERTION(!(object->m_flags & kImmutable), "object is already frozen");
    object->m_flags |= kImmutable;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrAccessStringKey(VMState *state) {
    const auto *instruction = (Instruction::AccessStringKey *)state->m_instr;

    const Slot objectSlot = instruction->m_objectSlot;
    const Slot targetSlot = instruction->m_targetSlot;

    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");

    Object *object = state->m_slots[objectSlot];

    const char *key = instruction->m_key;
    bool objectFound = false;

    state->m_slots[targetSlot] = Object::lookup(object, key, &objectFound);

    if (!objectFound) {
        Object *indexOperation = Object::lookup(object, "[]", nullptr);
        if (indexOperation) {
            Object *keyObject = Object::newString(state->m_restState, instruction->m_key, strlen(instruction->m_key));

            State subState = { };
            subState.m_parent = state->m_restState;
            subState.m_root = state->m_root;
            subState.m_shared = state->m_restState->m_shared;

            if (!VM::callCallable(&subState, object, indexOperation, &keyObject, 1)) {
                return { instrHalt };
            }

            VM::run(&subState);

            VM_ASSERTION(subState.m_runState != kErrored, "[] overload failed: %s\n",
                subState.m_error.c_str());

            state->m_slots[targetSlot] = subState.m_resultValue;

            objectFound = true;
        }
    }
    if (!objectFound) {
        VM_ASSERTION(false, "property not found: '%s'", key);
    }
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrSetConstraintStringKey(VMState *state) {
    const auto *instruction = (Instruction::SetConstraintStringKey *)state->m_instr;
    const Slot objectSlot = instruction->m_objectSlot;
    const Slot constraintSlot = instruction->m_constraintSlot;
    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(constraintSlot < state->m_cf->m_count, "slot addressing error");
    Object *object = state->m_slots[objectSlot];
    Object *constraint = state->m_slots[constraintSlot];
    const char *error = Object::setConstraint(state->m_restState,
                                              object,
                                              instruction->m_key,
                                              instruction->m_keyLength,
                                              constraint);
    VM_ASSERTION(!error, error);
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrAssign(VMState *state) {
    const auto *instruction = (Instruction::Assign *)state->m_instr;

    const Slot objectSlot = instruction->m_objectSlot;
    const Slot valueSlot = instruction->m_valueSlot;
    const Slot keySlot = instruction->m_keySlot;

    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(valueSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(keySlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(state->m_slots[keySlot], "null key slot");

    Object *object = state->m_slots[objectSlot];
    Object *valueObject = state->m_slots[valueSlot];
    Object *keyObject = state->m_slots[keySlot];
    Object *stringBase = state->m_restState->m_shared->m_valueCache.m_stringBase;
    StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
    if (!stringKey) {
        Object *indexAssignOperation = Object::lookup(object, "[]=", nullptr);
        if (indexAssignOperation) {
            Object *keyValuePair[] = { state->m_slots[instruction->m_keySlot], valueObject };
            if (!VM::callCallable(state->m_restState, object, indexAssignOperation, keyValuePair, 2)) {
                return { instrHalt };
            }
            state->m_instr = (Instruction *)(instruction + 1);
            return { instrFunctions[state->m_instr->m_type] };
        }
        VM_ASSERTION(false, "key is not string");
    }

    char *key = stringKey->m_value;
    GC::addPermanent(state->m_restState, keyObject);
    const AssignType assignType = instruction->m_assignType;
    switch (assignType) {
        case kAssignPlain: {
            Object::setNormal(state->m_restState, object, key, valueObject);
            break;
        }
        case kAssignExisting: {
            const char *error = Object::setExisting(state->m_restState, object, key, valueObject);
            VM_ASSERTION(!error, "%s for '%s'", error, key);
            break;
        }
        case kAssignShadowing: {
            bool keySet = false;
            const char *error = Object::setShadowing(state->m_restState, object, key, valueObject, &keySet);
            VM_ASSERTION(!error, "%s for '%s'", error, key);
            VM_ASSERTION(keySet, "key '%s' not found in object", key);
            break;
        }
    }
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrAssignStringKey(VMState *state) {
    const auto *instruction = (Instruction::AssignStringKey *)state->m_instr;
    const Slot objectSlot = instruction->m_objectSlot;
    const Slot valueSlot = instruction->m_valueSlot;
    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(valueSlot < state->m_cf->m_count, "slot addressing error");
    Object *object = state->m_slots[objectSlot];
    Object *valueObject = state->m_slots[valueSlot];
    const char *key = instruction->m_key;
    AssignType assignType = instruction->m_assignType;
    switch (assignType) {
        case kAssignPlain: {
            Object::setNormal(state->m_restState, object, key, valueObject);
            break;
        }
        case kAssignExisting: {
            const char *error = Object::setExisting(state->m_restState, object, key, valueObject);
            VM_ASSERTION(!error, "%s for '%s'", error, key);
            break;
        }
        case kAssignShadowing: {
            bool keySet = false;
            const char *error = Object::setShadowing(state->m_restState, object, key, valueObject, &keySet);
            VM_ASSERTION(!error, "%s for '%s'", error, key);
            VM_ASSERTION(keySet, "key '%s' not found in object", key);
            break;
        }
    }
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrCall(VMState *state) {
    const auto *instruction = (Instruction::Call *)state->m_instr;

    const Slot functionSlot = instruction->m_functionSlot;
    const Slot thisSlot = instruction->m_thisSlot;

    const size_t argsLength = instruction->m_count;

    VM_ASSERTION(functionSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(thisSlot < state->m_cf->m_count, "slot addressing error");

    Object *thisObject_ = state->m_slots[thisSlot];
    Object *functionObject_ = state->m_slots[functionSlot];

    Object **arguments = nullptr;
    if (argsLength < 10) {
        arguments = state->m_restState->m_shared->m_valueCache.m_preallocatedArguments[argsLength];
    } else {
        arguments = (Object **)Memory::allocate(sizeof(Object *) * argsLength);
    }

    for (size_t i = 0; i < argsLength; i++) {
        const Slot argumentSlot = ((Slot *)(instruction + 1))[i];
        VM_ASSERTION(argumentSlot < state->m_cf->m_count, "slot addressing error");
        arguments[i] = state->m_slots[argumentSlot];
    }

    state->m_instr = (Instruction *)((Slot *)(instruction + 1) + instruction->m_count);
    state->m_cf->m_instructions = state->m_instr;

    if (!VM::callCallable(state->m_restState, thisObject_, functionObject_, arguments, argsLength)) {
        return { instrHalt };
    }

    if (state->m_restState->m_runState == kErrored) {
        if (argsLength >= 10) {
            Memory::free(arguments);
        }
        return { instrHalt };
    }

    if (argsLength >= 10) {
        Memory::free(arguments);
    }

    VMState::refresh(state);

    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrSaveResult(VMState *state) {
    const auto *instruction = (Instruction::SaveResult *)state->m_instr;
    const Slot saveSlot = instruction->m_targetSlot;
    VM_ASSERTION(saveSlot < state->m_cf->m_count, "slot addressing error");
    state->m_slots[saveSlot] = state->m_restState->m_resultValue;
    state->m_restState->m_resultValue = nullptr;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrReturn(VMState *state) {
    const auto *instruction = (Instruction::Return *)state->m_instr;
    const Slot returnSlot = instruction->m_returnSlot;
    VM_ASSERTION(returnSlot < state->m_cf->m_count, "slot addressing error");
    Object *result = state->m_slots[returnSlot];
    GC::delRoots(state->m_restState, &state->m_cf->m_root);
    VM::delFrame(state->m_restState);
    state->m_restState->m_resultValue = result;

    if (!state->m_restState->m_frame) {
        return { instrHalt };
    }

    VMState::refresh(state);

    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrBranch(VMState *state) {
    const auto *instruction = (Instruction::Branch *)state->m_instr;
    const size_t block = instruction->m_block;
    VM_ASSERTION(block < state->m_cf->m_function->m_body.m_count, "block addressing error");
    state->m_instr = (Instruction *)((unsigned char *)state->m_cf->m_function->m_body.m_instructions
        + state->m_cf->m_function->m_body.m_blocks[block].m_offset);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrTestBranch(VMState *state) {
    const auto *instruction = (Instruction::TestBranch *)state->m_instr;

    const Slot testSlot = instruction->m_testSlot;

    const size_t trueBlock = instruction->m_trueBlock;
    const size_t falseBlock = instruction->m_falseBlock;

    VM_ASSERTION(testSlot < state->m_cf->m_count, "slot addressing error");

    Object *testObject = state->m_slots[testSlot];

    Object *boolBase = state->m_restState->m_shared->m_valueCache.m_boolBase;
    Object *intBase = state->m_restState->m_shared->m_valueCache.m_intBase;
    Object *boolObject = Object::instanceOf(testObject, boolBase);
    Object *intObject = Object::instanceOf(testObject, intBase);

    bool test = false;
    if (boolObject) {
        if (((BoolObject *)boolObject)->m_value) {
            test = true;
        }
    } else if (intObject) {
        if (((IntObject *)intObject)->m_value != 0) {
            test = true;
        }
    } else {
        test = !!testObject;
    }

    const size_t targetBlock = test ? trueBlock : falseBlock;
    state->m_instr = (Instruction *)((unsigned char *)state->m_cf->m_function->m_body.m_instructions
        + state->m_cf->m_function->m_body.m_blocks[targetBlock].m_offset);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrDefineFastSlot(VMState *state) {
    const auto *instruction = (Instruction::DefineFastSlot *)state->m_instr;

    const Slot targetSlot = instruction->m_targetSlot;
    const Slot objectSlot = instruction->m_objectSlot;

    VM_ASSERTION(objectSlot < state->m_cf->m_count, "slot addressing error");

    Object *object = state->m_slots[objectSlot];
    Object **target = Object::lookupReferenceWithHash(object,
                                                      instruction->m_key,
                                                      instruction->m_keyLength,
                                                      instruction->m_keyHash);

    VM_ASSERTION(target, "key not in object");

    state->m_cf->m_fastSlots[targetSlot] = target;
    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrReadFastSlot(VMState *state) {
    const auto *instruction = (Instruction::ReadFastSlot *)state->m_instr;

    const Slot targetSlot = instruction->m_targetSlot;
    const Slot sourceSlot = instruction->m_sourceSlot;

    VM_ASSERTION(targetSlot < state->m_cf->m_count, "slot addressing error");
    VM_ASSERTION(sourceSlot < state->m_cf->m_fastSlotsCount, "fast slot addressing error");

    state->m_slots[targetSlot] = *state->m_cf->m_fastSlots[sourceSlot];

    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrWriteFastSlot(VMState *state) {
    const auto *instruction = (Instruction::WriteFastSlot *)state->m_instr;

    const Slot targetSlot = instruction->m_targetSlot;
    const Slot sourceSlot = instruction->m_sourceSlot;

    VM_ASSERTION(targetSlot < state->m_cf->m_fastSlotsCount, "fast slot addressing error");
    VM_ASSERTION(sourceSlot < state->m_cf->m_count, "slot addressing error");

    *state->m_cf->m_fastSlots[targetSlot] = state->m_slots[sourceSlot];

    state->m_instr = (Instruction *)(instruction + 1);
    return { instrFunctions[state->m_instr->m_type] };
}

static VMFnWrap instrHalt(VMState *state) {
    (void)state;
    return { instrHalt };
}

void VM::step(State *state) {
    VMState vmState;
    vmState.m_restState = state;
    vmState.m_root = state->m_root;

    VMState::refresh(&vmState);

    VMInstrFn fn = (VMInstrFn)instrFunctions[vmState.m_instr->m_type];
    size_t i = 0;
    for (; i < (size_t)s_cycle_stride && fn != instrHalt; i++) {
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
        fn = fn(&vmState).self;
    }
    state->m_shared->m_cycleCount += i * 9;
    if (U_LIKELY(state->m_frame)) {
        state->m_frame->m_instructions = vmState.m_instr;
    }
    recordProfile(state);
}

void VM::run(State *state) {
    U_ASSERT(state->m_runState == kTerminated || state->m_runState == kErrored);
    if (!state->m_frame) {
        return;
    }
    state->m_runState = kRunning;
    state->m_error.clear();
    if (!state->m_shared->m_valueCache.m_preallocatedArguments) {
        state->m_shared->m_valueCache.m_preallocatedArguments =
            (Object ***)Memory::allocate(sizeof(Object **) * 10);
        for (size_t i = 0; i < 10; i++) {
            state->m_shared->m_valueCache.m_preallocatedArguments[i] =
                (Object **)Memory::allocate(sizeof(Object *) * i);
        }
    }
    RootSet resultSet;
    GC::addRoots(state, &state->m_resultValue, 1, &resultSet);
    while (state->m_runState == kRunning) {
        step(state);
        if (!state->m_frame) {
            state->m_runState = kTerminated;
        }
    }
    GC::delRoots(state, &resultSet);
    // TODO: tear down value cache
}

bool VM::callCallable(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    Object *closureBase = state->m_shared->m_valueCache.m_closureBase;
    Object *functionBase = state->m_shared->m_valueCache.m_functionBase;
    auto *functionObject = (FunctionObject *)Object::instanceOf(function, functionBase);
    auto *closureObject = (ClosureObject *)Object::instanceOf(function, closureBase);
    if (!(functionObject || closureObject)) {
        VM::error(state, "object is not callable");
        return false;
    }
    if (functionObject)
        functionObject->m_function(state, self, (Object *)functionObject, arguments, count);
    else
        closureObject->m_function(state, self, (Object *)closureObject, arguments, count);
    return true;
}

void VM::callFunction(State *state, Object *context, UserFunction *function, Object **arguments, size_t count) {
    addFrame(state, function->m_slots, function->m_fastSlots);
    CallFrame *frame = state->m_frame;
    frame->m_function = function;
    frame->m_slots[1] = context;
    GC::addRoots(state, frame->m_slots, frame->m_count, &frame->m_root);

    if (frame->m_function->m_hasVariadicTail)
        VM_ASSERT(count >= frame->m_function->m_arity, "arity violation in call");
    else
        VM_ASSERT(count == frame->m_function->m_arity, "arity violation in call");

    // TODO: check if this is correct for the context
    for (size_t i = 0; i < function->m_arity; i++)
        frame->m_slots[i + 2] = arguments[i];

    VM_ASSERT(frame->m_function->m_body.m_count, "invalid function");
    frame->m_instructions = frame->m_function->m_body.m_instructions;
}

Object *VM::setupVaradicArguments(State *state, Object *context, UserFunction *userFunction, Object **arguments, size_t count) {
    if (!userFunction->m_hasVariadicTail)
        return context;
    context = Object::newObject(state, context);
    U_ASSERT(count >= userFunction->m_arity);
    const size_t length = count - userFunction->m_arity;
    Object **allArguments = (Object **)Memory::allocate(sizeof *allArguments * length);
    for (size_t i = 0; i < length; i++)
        allArguments[i] = arguments[userFunction->m_arity + i];
    Object::setNormal(state, context, "$", Object::newArray(state, allArguments, (IntObject *)Object::newInt(state, length)));
    context->m_flags |= kClosed;
    return context;
}

void VM::functionHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)self;
    ClosureObject *functionObject = (ClosureObject *)function;
    Object *context = functionObject->m_context;
    GC::disable(state);
    context = setupVaradicArguments(state, context, &functionObject->m_closure, arguments, count);
    callFunction(state, context, &functionObject->m_closure, arguments, count);
    GC::enable(state);
}

void VM::methodHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    ClosureObject *functionObject = (ClosureObject *)function;
    Object *context = Object::newObject(state, functionObject->m_context);
    Object::setNormal(state, context, "this", self);
    context->m_flags |= kClosed;
    GC::disable(state);
    context = setupVaradicArguments(state, context, &functionObject->m_closure, arguments, count);
    callFunction(state, context, &functionObject->m_closure, arguments, count);
    GC::enable(state);
}

Object *Object::newClosure(State *state, Object *context, UserFunction *function) {
    ClosureObject *closureObject = (ClosureObject *)Memory::allocate(sizeof *closureObject, 1);
    closureObject->m_parent = state->m_shared->m_valueCache.m_closureBase;
    if (function->m_isMethod)
        closureObject->m_function = VM::methodHandler;
    else
        closureObject->m_function = VM::functionHandler;
    closureObject->m_context = context;
    closureObject->m_closure = *function;
    return (Object *)closureObject;
}

/* Profiling */
struct ProfileRecord {
    const char *m_textFrom;
    const char *m_textTo;
    int m_numSamples;
    bool m_direct;
};

struct OpenRange {
    ProfileRecord *m_record;
    OpenRange *m_prev;
    static void pushRecord(OpenRange **range, ProfileRecord *record);
    static void dropRecord(OpenRange **range);
    static void dropRecords(OpenRange **range);
};

void OpenRange::pushRecord(OpenRange **range, ProfileRecord *record) {
    OpenRange *newRange = (OpenRange *)Memory::allocate(sizeof *newRange);
    newRange->m_record = record;
    newRange->m_prev = *range;
    *range = newRange;
}

void OpenRange::dropRecord(OpenRange **range) {
    OpenRange *prev = (*range)->m_prev;
    Memory::free(*range);
    *range = prev;
}

void OpenRange::dropRecords(OpenRange **range) {
    for (; *range; OpenRange::dropRecord(range))
        ;
}

void ProfileState::dump(SourceRange source, ProfileState *profileState) {
    if (!s_profile)
        return;

    Table *directTable = &profileState->m_directTable;
    Table *indirectTable = &profileState->m_indirectTable;
    const size_t numDirectRecords = directTable->m_fieldsStored;
    const size_t numIndirectRecords = indirectTable->m_fieldsStored;
    const size_t numRecords = numDirectRecords + numIndirectRecords;

    ProfileRecord *recordEntries = (ProfileRecord *)Memory::allocate(sizeof *recordEntries, numRecords);

    int maxSamplesDirect = 0;
    int sumSamplesDirect = 0;
    int maxSamplesIndirect = 0;
    int sumSamplesIndirect = 0;
    int k = 0;

    // Calculate direct samples
    for (size_t i = 0; i < directTable->m_fieldsNum; ++i) {
        Field *field = &directTable->m_fields[i];
        if (field->m_name) {
            FileRange *range = *(FileRange **)field->m_name;
            const int samples = (intptr_t)field->m_value;
            if (samples > maxSamplesDirect)
                maxSamplesDirect = samples;
            sumSamplesDirect += samples;
            // add a direct entry
            recordEntries[k++] = {
                range->m_textFrom,
                range->m_textTo,
                samples,
                true
            };
        }
    }

    // Calculate indirect samples
    for (size_t i = 0; i < indirectTable->m_fieldsNum; i++) {
        Field *field = &indirectTable->m_fields[i];
        if (field->m_name) {
            FileRange *range = *(FileRange **)field->m_name;
            const int samples = (intptr_t)field->m_value;
            if (samples > maxSamplesIndirect)
                maxSamplesIndirect = samples;
            sumSamplesIndirect += samples;
            // add a direct entry
            recordEntries[k++] = {
                range->m_textFrom,
                range->m_textTo,
                samples,
                false
            };
        }
    }

    U_ASSERT(k == (int)numRecords);

    qsort(recordEntries, numRecords, sizeof(ProfileRecord), [](const void *a, const void *b) -> int {
        const ProfileRecord *recordA = (const ProfileRecord *)a;
        const ProfileRecord *recordB = (const ProfileRecord *)b;

        // Ranges which start early should come first
        if (recordA->m_textFrom < recordB->m_textFrom)
            return -1;
        if (recordA->m_textFrom > recordB->m_textFrom)
            return 1;

        // Ranges that end later which are outermost
        if (recordA->m_textTo > recordB->m_textTo)
            return -1;
        if (recordA->m_textTo < recordB->m_textTo)
            return 1;

        // Ranges are identical (we have a direct/indirect collision)
        return 0;
    });

    // Generate the sampling profile
    OpenRange *openRangeHead = nullptr;

    u::file dump = u::fopen(s_profile_file, "w");
    u::fprint(dump, "<!DOCTYPE html>\n");
    u::fprint(dump, "<html>\n");
    u::fprint(dump, "<head>\n");
    u::fprint(dump, "<style>\n");
    u::fprint(dump, "span { position: relative; }\n");
    u::fprint(dump, "</style>\n");
    u::fprint(dump, "</head>\n");
    u::fprint(dump, "<body>\n");
    u::fprint(dump, "<pre>\n");

    char *currentCharacter = source.m_begin;
    int spanIndex = 100000;
    size_t currentEntryIndex = 0;
    while (currentCharacter != source.m_end) {
        // Close all
        while (openRangeHead && openRangeHead->m_record->m_textTo == currentCharacter) {
            OpenRange::dropRecord(&openRangeHead);
            u::fprint(dump, "</span>");
        }
        // Skip any preceding
        while (currentEntryIndex < numRecords && recordEntries[currentEntryIndex].m_textFrom < currentCharacter)
            currentEntryIndex++;
        // Open any new
        while (currentEntryIndex < numRecords && recordEntries[currentEntryIndex].m_textFrom == currentCharacter) {
            OpenRange::pushRecord(&openRangeHead, &recordEntries[currentEntryIndex]);
            int *samplesDirectSearch = nullptr;
            int *samplesIndirectSearch = nullptr;
            OpenRange *current = openRangeHead;
            // Search for the innermost direct or indirect values
            while (current && (!samplesDirectSearch || !samplesIndirectSearch)) {
                if (!samplesDirectSearch && current->m_record->m_direct)
                    samplesDirectSearch = &current->m_record->m_numSamples;
                if (!samplesIndirectSearch && !current->m_record->m_direct)
                    samplesIndirectSearch = &current->m_record->m_numSamples;
                current = current->m_prev;
            }
            int samplesDirect = samplesDirectSearch ? *samplesDirectSearch : 0;
            int samplesIndirect = samplesIndirectSearch ? *samplesIndirectSearch : 0;
            double percentDirect = (samplesDirect * 100.0) / sumSamplesDirect;
            int hexDirect = 255 - (samplesDirect * 255LL) / maxSamplesDirect;
            double percentIndirect = (samplesIndirect * 100.0) / sumSamplesDirect;
            int weightIndirect = 100 + 100 * ((samplesIndirect * 8LL) / sumSamplesDirect);
            float borderIndirect = samplesIndirect * 3.0 / sumSamplesDirect;
            int fontSizeIndirect = 100 + (samplesIndirect * 10LL) / sumSamplesDirect;
            int borderColumnIndirect = 15 - (int)(15 * ((borderIndirect < 1) ? borderIndirect : 1));
            u::fprint(dump, "<span title=\"%.2f%% active, %.2f%% in backtrace\""
                " style =\"",
                percentDirect,
                percentIndirect);
            if (hexDirect <= 250)
                u::fprint(dump, "background-color:#ff%02x%02x;", hexDirect, hexDirect);
            u::fprint(dump, "font-weight:%d; border-bottom:%fpx solid #%1x%1x%1x; font-size: %d%%;",
                weightIndirect,
                borderIndirect,
                borderColumnIndirect,
                borderColumnIndirect,
                borderColumnIndirect,
                fontSizeIndirect);
            u::fprint(dump, "z-index: %d;", --spanIndex);
            u::fprint(dump, "\">");
            currentEntryIndex++;
        }
        // Close all the zero size ones
        while (openRangeHead && openRangeHead->m_record->m_textTo == currentCharacter) {
            OpenRange::dropRecord(&openRangeHead);
            u::fprint(dump, "</span>");
        }
        if (*currentCharacter == '<')
            u::fprint(dump, "&lt;");
        else if (*currentCharacter == '>')
            u::fprint(dump, "&gt;");
        else
            u::fprint(dump, "%c", *currentCharacter);
        currentCharacter++;
    }
    u::fprint(dump, "</pre>\n");
    u::fprint(dump, "</body>\n");
    u::fprint(dump, "</html>\n");

    OpenRange::dropRecords(&openRangeHead);
}

}
