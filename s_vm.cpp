#include <stdarg.h>

#include "s_object.h"
#include "s_gc.h"
#include "s_vm.h"

#include "u_assert.h"
#include "u_misc.h"

namespace s {

CallFrame *VM::addFrame(State *state, size_t slots) {
    void *stack = (void *)state->m_stack;
    if (stack) {
        size_t capacity = *((size_t *)stack - 1);
        size_t newSize = sizeof(CallFrame) * (state->m_length + 1);
        if (newSize > capacity) {
            size_t newCapacity = capacity * 2;
            if (newSize > newCapacity)
                newCapacity = newSize;
            void *old = stack;
            void *base = (void *)((CallFrame*)stack - 1);
            stack = neoMalloc(newCapacity + sizeof(CallFrame));
            stack = (void *)((CallFrame *)stack + 1);
            *((size_t *)stack - 1) = newCapacity;
            // tear down old gc roots
            for (size_t i = 0; i < state->m_length; i++)
                GC::delRoots(state, &state->m_stack[i].m_root);
            memcpy(stack, old, capacity);
            state->m_stack = (CallFrame *)stack;
            // add new gc roots
            for (size_t i = 0; i < state->m_length; i++) {
                CallFrame *frame = &state->m_stack[i];
                GC::addRoots(state, frame->m_slots, frame->m_count, &frame->m_root);
            }
            neoFree(base);
        }
    } else {
        U_ASSERT(state->m_length == 0);
        size_t initialCapacity = sizeof(CallFrame);
        stack = neoMalloc(initialCapacity + sizeof(CallFrame));
        stack = (void *)((CallFrame *)stack + 1);
        *((size_t *)stack - 1) = initialCapacity;
        state->m_stack = (CallFrame *)stack;
    }
    state->m_length = state->m_length + 1;
    CallFrame *frame = &state->m_stack[state->m_length - 1];
    frame->m_count = slots;
    frame->m_slots = (Object **)neoCalloc(sizeof(Object *), slots);
    return frame;
}

void VM::delFrame(State *state) {
    CallFrame *frame = &state->m_stack[state->m_length - 1];
    neoFree(frame->m_slots);
    state->m_length = state->m_length - 1;
}

void VM::error(State *state, const char *fmt, ...) {
    U_ASSERT(state->m_runState == kRunning);
    va_list va;
    va_start(va, fmt);
    state->m_error = u::format(fmt, va);
    va_end(va);
    state->m_runState = kErrored;
}

#define I(NAME, TYPE) \
    Instruction::TYPE *NAME = (Instruction::TYPE *)instruction

#define BEGIN {
#define BREAK } break

void VM::step(State *state, void **preallocatedArguments) {
    Object *root = state->m_root;
    CallFrame *frame = &state->m_stack[state->m_length - 1];
    Instruction *instruction = frame->m_instructions;
    Instruction *nextInstruction = nullptr;
    switch (instruction->m_type) {
    case kGetRoot: BEGIN
        I(i, GetRoot);
        VM_ASSERT(i->m_slot < frame->m_count, "slot addressing error");
        frame->m_slots[i->m_slot] = root;
        nextInstruction = (Instruction *)(i + 1);
        BREAK;

    case kGetContext: BEGIN
        I(i, GetContext);
        VM_ASSERT(i->m_slot < frame->m_count, "slot addressing error");
        frame->m_slots[i->m_slot] = frame->m_context;
        nextInstruction = (Instruction *)(i + 1);
        BREAK;

    case kAccess:
    case kAccessStringKey: BEGIN
        I(access, Access);
        I(accessStringKey, AccessStringKey);
        Slot objectSlot;
        Slot targetSlot;
        if (instruction->m_type == kAccess) {
            objectSlot = access->m_objectSlot;
            targetSlot = access->m_targetSlot;
            nextInstruction = (Instruction *)(access + 1);
        } else {
            objectSlot = accessStringKey->m_objectSlot;
            targetSlot = accessStringKey->m_targetSlot;
            nextInstruction = (Instruction *)(accessStringKey + 1);
        }

        VM_ASSERT(objectSlot < frame->m_count, "slot addressing error");
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");

        Object *object = frame->m_slots[objectSlot];

        const char *key = nullptr;
        bool hasKey = false;
        if (instruction->m_type == kAccess) {
            Slot keySlot = access->m_keySlot;

            VM_ASSERT(keySlot < frame->m_count, "slot addressing error");
            VM_ASSERT(frame->m_slots[keySlot], "null key slot");

            Object *stringBase = Object::lookup(root, "string", nullptr);
            Object *keyObject = frame->m_slots[keySlot];

            VM_ASSERT(keyObject, "key is null");

            StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
            if (stringKey) {
                key = stringKey->m_value;
                hasKey = true;
            }
        } else {
            key = accessStringKey->m_key;
            hasKey = true;
        }

        bool objectFound = false;
        if (hasKey)
            frame->m_slots[targetSlot] = Object::lookup(object, key, &objectFound);
        if (!objectFound) {
            Object *indexOperation = Object::lookup(object, "[]", nullptr);
            if (indexOperation) {
                Object *functionBase = Object::lookup(root, "function", nullptr);
                Object *closureBase = Object::lookup(root, "closure", nullptr);

                FunctionObject *functionIndexOperation = (FunctionObject *)Object::instanceOf(indexOperation, functionBase);
                ClosureObject *closureIndexOperation = (ClosureObject *)Object::instanceOf(indexOperation, closureBase);

                VM_ASSERT(functionIndexOperation || closureIndexOperation,
                    "index operation isn't callable");

                Object *keyObject;
                if (instruction->m_type == kAccess)
                    keyObject = frame->m_slots[access->m_keySlot];
                else
                    keyObject = Object::newString(state, accessStringKey->m_key);

                // setup another virtual machine to place this call
                State substate;
                memset(&substate, 0, sizeof substate);

                substate.m_root = state->m_root;
                substate.m_gc = state->m_gc;

                if (functionIndexOperation)
                    functionIndexOperation->m_function(&substate, object, indexOperation, &keyObject, 1);
                else if (closureIndexOperation)
                    closureIndexOperation->m_function(&substate, object, indexOperation, &keyObject, 1);

                run(&substate);
                VM_ASSERT(substate.m_runState != kErrored, "overload failed '[]' %s", &substate.m_error[0]);

                frame->m_slots[targetSlot] = substate.m_result;

                objectFound = true;
            }
        }
        if (!objectFound) {
            if (hasKey)
                VM_ASSERT(false, "property not found: '%s'", key);
            else
                VM_ASSERT(false, "property not found");
            U_UNREACHABLE();
        }
        BREAK;

    case kAssign:
    case kAssignStringKey: BEGIN
        I(assign, Instruction::Assign);
        I(assignStringKey, Instruction::AssignStringKey);

        AssignType assignType;
        Object *object;
        Object *valueObject;
        const char *key;
        bool hasKey = false;

        if (instruction->m_type == kAssign) {
            Slot objectSlot = assign->m_objectSlot;
            Slot valueSlot = assign->m_valueSlot;
            Slot keySlot = assign->m_keySlot;

            VM_ASSERT(objectSlot < frame->m_count, "slot addressing error");
            VM_ASSERT(valueSlot < frame->m_count, "slot addressing error");
            VM_ASSERT(keySlot < frame->m_count, "slot addressing error");
            VM_ASSERT(frame->m_slots[keySlot], "null key slot");

            object = frame->m_slots[objectSlot];
            valueObject = frame->m_slots[valueSlot];

            Object *stringBase = Object::lookup(root, "string", nullptr);
            Object *keyObject = frame->m_slots[keySlot];
            StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
            if (stringKey) {
                key = stringKey->m_value;
                assignType = assign->m_assignType;
                hasKey = true;
            }
            nextInstruction = (Instruction *)(assign + 1);
        } else {
            Slot objectSlot = assignStringKey->m_objectSlot;
            Slot valueSlot = assignStringKey->m_valueSlot;
            VM_ASSERT(objectSlot < frame->m_count, "slot addressing error");
            VM_ASSERT(valueSlot < frame->m_count, "slot addressing error");
            object = frame->m_slots[objectSlot];
            valueObject = frame->m_slots[valueSlot];
            key = assignStringKey->m_key;
            assignType = assignStringKey->m_assignType;
            hasKey = true;
            nextInstruction = (Instruction *)(assignStringKey + 1);
        }
        if (!hasKey) {
            Object *indexAssignOperation = Object::lookup(object, "[]=", nullptr);
            if (indexAssignOperation) {
                Object *functionBase = Object::lookup(root, "function", nullptr);
                Object *closureBase = Object::lookup(root, "closure", nullptr);

                FunctionObject *functionIndexAssignOperation =
                    (FunctionObject *)Object::instanceOf(indexAssignOperation, functionBase);
                ClosureObject *closureIndexAssignOperation =
                    (ClosureObject *)Object::instanceOf(indexAssignOperation, closureBase);

                VM_ASSERT(functionIndexAssignOperation || closureIndexAssignOperation,
                    "index assign operation isn't callable");

                Object *keyValuePair[] = { frame->m_slots[assign->m_keySlot], valueObject };
                if (functionIndexAssignOperation)
                    functionIndexAssignOperation->m_function(state, object, indexAssignOperation, keyValuePair, 2);
                else
                    closureIndexAssignOperation->m_function(state, object, indexAssignOperation, keyValuePair, 2);
                break;
            }
            VM_ASSERT(false, "key is not string");
        }
        switch (assignType) {
        case kAssignPlain:
            if (!object)
                VM_ASSERT(false, "assignment to null object");
            Object::setNormal(object, key, valueObject);
            break;
        case kAssignExisting:
            if (!Object::setExisting(object, key, valueObject))
                VM_ASSERT(false, "key '%s' not found in object", key);
            break;
        case kAssignShadowing:
            if (!Object::setShadowing(object, key, valueObject))
                VM_ASSERT(false, "key '%s' not found in object", key);
            break;
        }
        BREAK;

    case kNewObject: BEGIN
        I(newObject, Instruction::NewObject);

        Slot targetSlot = newObject->m_targetSlot;
        Slot parentSlot = newObject->m_parentSlot;

        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        VM_ASSERT(parentSlot < frame->m_count, "slot addressing error");

        Object *parentObject = frame->m_slots[parentSlot];
        if (parentObject) {
            VM_ASSERT(!(parentObject->m_flags & kNoInherit),
                "cannot inherit from this object");
        }
        frame->m_slots[targetSlot] = Object::newObject(state, parentObject);
        nextInstruction = (Instruction *)(newObject + 1);
    BREAK;

    case kNewIntObject: BEGIN
        I(newIntObject, Instruction::NewIntObject);
        Slot targetSlot = newIntObject->m_targetSlot;
        int value = newIntObject->m_value;
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        frame->m_slots[targetSlot] = Object::newInt(state, value);
        nextInstruction = (Instruction *)(newIntObject + 1);
    BREAK;


    case kNewFloatObject: BEGIN
        I(newFloatObject, Instruction::NewFloatObject);
        Slot targetSlot = newFloatObject->m_targetSlot;
        float value = newFloatObject->m_value;
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        frame->m_slots[targetSlot] = Object::newFloat(state, value);
        nextInstruction = (Instruction *)(newFloatObject + 1);
    BREAK;

    case kNewArrayObject: BEGIN
        I(newArrayObject, Instruction::NewArrayObject);
        Slot targetSlot = newArrayObject->m_targetSlot;
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        frame->m_slots[targetSlot] = Object::newArray(state, nullptr, 0);
        nextInstruction = (Instruction *)(newArrayObject + 1);
    BREAK;

    case kNewStringObject: BEGIN
        I(newStringObject, Instruction::NewStringObject);
        Slot targetSlot = newStringObject->m_targetSlot;
        const char *value = newStringObject->m_value;
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        frame->m_slots[targetSlot] = Object::newString(state, value);
        nextInstruction = (Instruction *)(newStringObject + 1);
    BREAK;

    case kNewClosureObject: BEGIN
        I(newClosureObject, Instruction::NewClosureObject);
        Slot targetSlot = newClosureObject->m_targetSlot;
        Slot contextSlot = newClosureObject->m_contextSlot;
        VM_ASSERT(targetSlot < frame->m_count, "slot addressing error");
        VM_ASSERT(contextSlot < frame->m_count, "slot addressing error");
        frame->m_slots[targetSlot] = Object::newClosure(frame->m_slots[contextSlot], newClosureObject->m_function);
        nextInstruction = (Instruction *)(newClosureObject + 1);
    BREAK;

    case kCloseObject: BEGIN
        I(closeObject, Instruction::CloseObject);
        Slot slot = closeObject->m_slot;
        VM_ASSERT(slot < frame->m_count, "slot addressing error");
        Object *object = frame->m_slots[slot];
        VM_ASSERT(!(object->m_flags & kClosed), "object is already closed");
        object->m_flags |= kClosed;
        nextInstruction = (Instruction *)(closeObject + 1);
        BREAK;

    case kCall: BEGIN
        I(call, Instruction::Call);

        Slot functionSlot = call->m_functionSlot;
        Slot thisSlot = call->m_thisSlot;

        size_t count = call->m_count;

        VM_ASSERT(functionSlot < frame->m_count, "slot addressing error");
        VM_ASSERT(thisSlot < frame->m_count, "slot addressing error");

        Object *thisObject = frame->m_slots[thisSlot];
        Object *funObject = frame->m_slots[functionSlot];

        Object *functionBase = Object::lookup(root, "function", nullptr);
        Object *closureBase = Object::lookup(root, "closure", nullptr);

        FunctionObject *functionObject = (FunctionObject *)Object::instanceOf(funObject, functionBase);
        ClosureObject *closureObject = (ClosureObject *)Object::instanceOf(funObject, closureBase);

        VM_ASSERT(functionObject || closureObject,
            "object isn't callable");

        Object **arguments = count < 10
            ? (Object **)preallocatedArguments[count]
            : (Object **)neoMalloc(sizeof(Object *) * count);

        for (size_t i = 0; i < count; i++) {
            Slot argumentSlot = call->m_arguments[i];
            VM_ASSERT(argumentSlot < frame->m_count, "slot addressing error");
            arguments[i] = frame->m_slots[argumentSlot];
        }

        size_t previousStackLength = state->m_length;

        if (functionObject)
            functionObject->m_function(state, thisObject, funObject, arguments, count);
        else
            closureObject->m_function(state, thisObject, funObject, arguments, count);

        // m_function may have triggered a stack reallocation which means a different
        // frame location
        frame = (CallFrame *)&state->m_stack[previousStackLength - 1];

        if (count >= 10)
            neoFree(arguments);

        nextInstruction = (Instruction *)(call + 1);
        BREAK;

    case kReturn: BEGIN
        I(ret, Instruction::Return);
        Slot returnSlot = ret->m_returnSlot;
        VM_ASSERT(returnSlot < frame->m_count, "slot addressing error");
        Object *result = frame->m_slots[returnSlot];
        GC::delRoots(state, &frame->m_root);
        delFrame(state);
        state->m_result = result;
        nextInstruction = (Instruction *)(ret + 1);
        BREAK;

    case kSaveResult: BEGIN
        I(saveResult, Instruction::SaveResult);
        Slot saveSlot = saveResult->m_targetSlot;
        VM_ASSERT(saveSlot < frame->m_count, "slot addressing error");
        frame->m_slots[saveSlot] = state->m_result;
        state->m_result = nullptr;
        nextInstruction = (Instruction *)(saveResult + 1);
        BREAK;

    case kBranch: BEGIN
        I(branch, Instruction::Branch);
        size_t block = branch->m_block;
        VM_ASSERT(block < frame->m_function->m_body.m_count, "block addressing error");
        nextInstruction = frame->m_function->m_body.m_blocks[block].m_instructions;
        BREAK;

    case kTestBranch: BEGIN
        I(testBranch, Instruction::TestBranch);
        Slot testSlot = testBranch->m_testSlot;
        size_t trueBlock = testBranch->m_trueBlock;
        size_t falseBlock = testBranch->m_falseBlock;
        VM_ASSERT(testSlot < frame->m_count, "slot addressing error");
        Object *testObject = frame->m_slots[testSlot];

        Object *boolBase = Object::lookup(root, "bool", nullptr);
        Object *intBase = Object::lookup(root, "int", nullptr);
        Object *boolObject = Object::instanceOf(testObject, boolBase);
        Object *intObject = Object::instanceOf(testObject, intBase);

        bool test = false;
        if (boolObject) {
            if (((BoolObject *)boolObject)->m_value)
                test = true;
        } else if (intObject) {
            if (((IntObject *)intObject)->m_value != 0)
                test = true;
        } else {
            test = !!testObject;
        }

        const size_t targetBlock = test ? trueBlock : falseBlock;
        nextInstruction = frame->m_function->m_body.m_blocks[targetBlock].m_instructions;
        BREAK;

    default:
        U_ASSERT(0 && "invalid instruction");
        break;
    }
    if (state->m_runState == kErrored)
        return;
    frame->m_instructions = nextInstruction;
}

void VM::run(State *state) {
    U_ASSERT(state->m_runState == kTerminated || state->m_runState == kErrored);
    if (state->m_length == 0)
        return;
    U_ASSERT(state->m_length);
    state->m_runState = kRunning;
    state->m_error.clear();
    // preallocate space for 10 arguments to place function calls
    void **arguments = (void **)neoMalloc(sizeof(void *) * 10);
    for (size_t i = 0; i < 10; i++)
        arguments[i] = neoMalloc(sizeof(Object *) * i);
    while (state->m_runState == kRunning) {
        step(state, arguments);
        if (state->m_length == 0)
            state->m_runState = kTerminated;
    }
}

void VM::callFunction(State *state, Object *context, UserFunction *function, Object **arguments, size_t count) {
    CallFrame *frame = addFrame(state, function->m_slots);
    frame->m_function = function;
    frame->m_context = context;
    GC::addRoots(state, frame->m_slots, frame->m_count, &frame->m_root);

    VM_ASSERT(count == frame->m_function->m_arity, "arity violation in call");
    for (size_t i = 0; i < count; i++)
        frame->m_slots[i] = arguments[i];

    VM_ASSERT(frame->m_function->m_body.m_count, "invalid function");
    frame->m_instructions = frame->m_function->m_body.m_blocks[0].m_instructions;
}

void VM::functionHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)self;
    ClosureObject *functionObject = (ClosureObject *)function;
    callFunction(state, functionObject->m_context, &functionObject->m_closure, arguments, count);
}

void VM::methodHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    ClosureObject *functionObject = (ClosureObject *)function;
    Object *context = Object::newObject(state, functionObject->m_context);
    Object::setNormal(context, "this", self);
    callFunction(state, context, &functionObject->m_closure, arguments, count);
}

Object *Object::newClosure(Object *context, UserFunction *function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = Object::lookup(root, "closure", nullptr);
    ClosureObject *closureObject = (ClosureObject *)neoCalloc(sizeof *closureObject, 1);
    closureObject->m_parent = closureBase;
    if (function->m_isMethod)
        closureObject->m_function = VM::methodHandler;
    else
        closureObject->m_function = VM::functionHandler;
    closureObject->m_context = context;
    closureObject->m_closure = *function;
    return (Object *)closureObject;
}

}
