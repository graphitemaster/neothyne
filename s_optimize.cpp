#include "s_gen.h"
#include "s_instr.h"
#include "s_memory.h"
#include "s_optimize.h"

#include "u_log.h"

namespace s {

static inline void copyFunctionStats(UserFunction *from, UserFunction *to) {
    to->m_slots = from->m_slots;
    to->m_fastSlots = from->m_fastSlots;
    to->m_arity = from->m_arity;
    to->m_name = from->m_name;
    to->m_isMethod = from->m_isMethod;
    to->m_hasVariadicTail = from->m_hasVariadicTail;
}

// Searches for primitive slots for a given function.
//
// Primitives slots are slots whose value is only used as a parameter to
// other instructions and does not escape.
static inline void findPrimitiveSlots(UserFunction *function, bool **slots) {
    *slots = (bool *)Memory::allocate(sizeof **slots * function->m_slots);
    for (size_t i = 0; i < function->m_slots; i++)
        (*slots)[i] = true;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        Instruction *instruction = InstructionBlock::begin(function, i);
        Instruction *instructionsEnd = InstructionBlock::end(function, i);
        while (instruction != instructionsEnd) {
            switch (instruction->m_type) {
            case kNewObject:
                (*slots)[((Instruction::NewObject*)instruction)->m_parentSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::NewObject));
                break;
            case kAccess:
                (*slots)[((Instruction::Access*)instruction)->m_objectSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::Access));
                break;
            case kAssign:
                (*slots)[((Instruction::Assign*)instruction)->m_objectSlot] = false;
                (*slots)[((Instruction::Assign*)instruction)->m_valueSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::Assign));
                break;
            case kSetConstraint:
                (*slots)[((Instruction::SetConstraint *)instruction)->m_objectSlot] = false;
                (*slots)[((Instruction::SetConstraint *)instruction)->m_constraintSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::SetConstraint));
                break;
            case kCall:
                (*slots)[((Instruction::Call*)instruction)->m_functionSlot] = false;
                (*slots)[((Instruction::Call*)instruction)->m_thisSlot] = false;
                for (size_t i = 0; i < ((Instruction::Call*)instruction)->m_count; i++)
                    (*slots)[((Instruction::Call*)instruction)->m_arguments[i]] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::Call));
                break;
            case kReturn:
                (*slots)[((Instruction::Return*)instruction)->m_returnSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::Return));
                break;
            case kTestBranch:
                (*slots)[((Instruction::TestBranch*)instruction)->m_testSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::TestBranch));
                break;
            default:
                instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
                break;
            }
        }
    }
}

struct SlotObjectInfo {
    bool m_staticObject;
    Slot m_parentSlot;
    const char **m_namesData;
    size_t m_namesLength;
    FileRange *m_belongsTo;
    Slot m_contextSlot;
    Instruction *m_afterObjectDecl;
};

// Searches for static object slots; which are any objects that are marked closed.
// Closed objects have the benefit that they cannot be futher added to so we can
// perform fast slot accesses on instead of the usual technique.
static inline void findStaticObjectSlots(UserFunction *function, SlotObjectInfo **slots) {
    *slots = (SlotObjectInfo *)Memory::allocate(sizeof **slots, function->m_slots);
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        Instruction *instruction = InstructionBlock::begin(function, i);
        Instruction *instructionEnd = InstructionBlock::end(function, i);
        while (instruction != instructionEnd) {
            if (instruction->m_type == kNewObject) {
                const auto *newObjectInstruction = (Instruction::NewObject *)instruction;
                instruction = (Instruction *)(newObjectInstruction + 1);
                bool failed = false;
                const char **namesData = nullptr;
                size_t namesLength = 0;
                while (instruction != instructionEnd) {
                    if (instruction->m_type == kAssignStringKey) {
                        const auto *assignStringKeyInstruction = (Instruction::AssignStringKey *)instruction;
                        if (assignStringKeyInstruction->m_assignType != kAssignPlain) {
                            failed = true;
                            break;
                        }
                        instruction = (Instruction *)(assignStringKeyInstruction + 1);
                        namesData = (const char **)Memory::reallocate(namesData, sizeof *namesData * ++namesLength);
                        namesData[namesLength - 1] = assignStringKeyInstruction->m_key;
                    } else if (instruction->m_type == kCloseObject) {
                        break;
                    } else {
                        failed = true;
                        break;
                    }
                }
                if (failed) {
                    Memory::free(namesData);
                    continue;
                }
                const Slot targetSlot = newObjectInstruction->m_targetSlot;
                (*slots)[targetSlot].m_staticObject = true;
                (*slots)[targetSlot].m_parentSlot = newObjectInstruction->m_parentSlot;
                (*slots)[targetSlot].m_namesData = namesData;
                (*slots)[targetSlot].m_namesLength = namesLength;
                (*slots)[targetSlot].m_belongsTo = instruction->m_belongsTo;
                (*slots)[targetSlot].m_contextSlot = instruction->m_contextSlot;

                instruction = (Instruction *)((Instruction::CloseObject *)instruction + 1);
                (*slots)[targetSlot].m_afterObjectDecl = instruction;
                continue;
            }
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }
}

// Redirect lookups which we know will fail in the current object to
// search the parent instead. This is used to avoid O(n) searches on
// inheritence chains which we can show statically are always going
// to fail.
UserFunction *Optimize::predictPass(UserFunction *function) {
    // TODO: audit and fix?
    SlotObjectInfo *info = nullptr;
    findStaticObjectSlots(function, &info);

    size_t count = 0;

    Gen gen = { };
    gen.m_slot = 1;
    gen.m_fastSlot = function->m_fastSlots;
    gen.m_blockTerminated = true;

    for (size_t i = 0; i < function->m_body.m_count; i++) {
        Gen::newBlock(&gen);
        Instruction *instruction = InstructionBlock::begin(function, i);
        Instruction *instructionsEnd = InstructionBlock::end(function, i);
        while (instruction != instructionsEnd) {
            const auto *accessStringKey = (Instruction::AccessStringKey *)instruction;
            if (instruction->m_type == kAccessStringKey) {
                Instruction::AccessStringKey newAccessStringKey = *accessStringKey;
                for (;;) {
                    const Slot objectSlot = newAccessStringKey.m_objectSlot;
                    if (!info[objectSlot].m_staticObject)
                        break;
                    bool keyInObject = false;
                    for (size_t k = 0; k < info[objectSlot].m_namesLength; k++) {
                        const char *objectKey = info[objectSlot].m_namesData[k];
                        if (strcmp(objectKey, newAccessStringKey.m_key) == 0) {
                            keyInObject = true;
                            break;
                        }
                    }
                    if (keyInObject)
                        break;
                    // So the key was not found in the object. We now know statically
                    // that the lookup will never succeed at runtime either.
                    // So automatically set the object slot to the parent. This
                    // allows us to skip searching up the object chain.
                    newAccessStringKey.m_objectSlot = info[objectSlot].m_parentSlot;
                    count++;
                }
                Gen::addLike(&gen, instruction, sizeof newAccessStringKey, (Instruction *)&newAccessStringKey);
                instruction = (Instruction *)(accessStringKey + 1);
                continue;
            }
            Gen::addLike(&gen, instruction, Instruction::size(instruction), instruction);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }
    Memory::free(info);
    UserFunction *optimized = Gen::buildFunction(&gen);
    copyFunctionStats(function, optimized);
    u::Log::out("[script] => redirected %zu predictable lookup misses\n", count);
    return optimized;
}

// For variables of which it's possible; replace accesses and assignments
// through usual slot addressing with fast slots instead. Fast slots are
// effectively register renames in the VM.
UserFunction *Optimize::fastSlotPass(UserFunction *function) {
    SlotObjectInfo *info = nullptr;
    findStaticObjectSlots(function, &info);

    size_t infoSlotsLength = 0;
    for (size_t i = 0; i < function->m_slots; i++)
        if (info[i].m_staticObject)
            infoSlotsLength++;

    u::vector<Slot> infoSlots(infoSlotsLength);
    u::vector<bool> objectFastSlotsInitialized(function->m_slots);
    u::vector<u::vector<Slot>> fastSlots(function->m_slots);
    size_t k = 0;
    for (Slot i = 0; i < function->m_slots; i++) {
        if (info[i].m_staticObject) {
            infoSlots[k++] = i;
            fastSlots[i].resize(info[i].m_namesLength);
        }
    }

    Gen gen = { };
    gen.m_slot = 1;
    gen.m_fastSlot = function->m_fastSlots;
    gen.m_blockTerminated = true;

    size_t defines = 0;
    size_t reads = 0;
    size_t writes = 0;

    for (size_t i = 0; i < function->m_body.m_count; i++) {
        Gen::newBlock(&gen);
        Instruction *instruction = InstructionBlock::begin(function, i);
        Instruction *instructionsEnd = InstructionBlock::end(function, i);
        while (instruction != instructionsEnd) {
            for (size_t k = 0; k < infoSlotsLength; k++) {
                const Slot slot = infoSlots[k];
                if (instruction == info[slot].m_afterObjectDecl) {
                    Gen::useRangeStart(&gen, info[slot].m_belongsTo);
                    gen.m_scope = info[slot].m_contextSlot;
                    for (size_t l = 0; l < info[slot].m_namesLength; l++) {
                        fastSlots[slot][l] = Gen::addDefineFastSlot(&gen, slot, info[slot].m_namesData[l]);
                        defines++;
                    }
                    Gen::useRangeEnd(&gen, info[slot].m_belongsTo);
                    objectFastSlotsInitialized[slot] = true;
                }
            }
            if (instruction->m_type == kAccessStringKey) {
                const auto *accessStringKeyInstruction = (Instruction::AccessStringKey *)instruction;
                const Slot objectSlot = accessStringKeyInstruction->m_objectSlot;
                const char *key = accessStringKeyInstruction->m_key;
                if (info[objectSlot].m_staticObject && objectFastSlotsInitialized[objectSlot]) {
                    for (size_t k = 0; k < info[objectSlot].m_namesLength; k++) {
                        const char *name = info[objectSlot].m_namesData[k];
                        if (!strcmp(key, name)) {
                            const Slot fastSlot = fastSlots[objectSlot][k];
                            Gen::useRangeStart(&gen, instruction->m_belongsTo);
                            gen.m_scope = instruction->m_contextSlot;
                            Gen::addReadFastSlot(&gen, fastSlot, accessStringKeyInstruction->m_targetSlot);
                            reads++;
                            Gen::useRangeEnd(&gen, instruction->m_belongsTo);
                            instruction = (Instruction *)(accessStringKeyInstruction + 1);
                            continue;
                        }
                    }
                }
            }
            if (instruction->m_type == kAssignStringKey) {
                const auto *assignStringKeyInstruction = (Instruction::AssignStringKey *)instruction;
                const Slot objectSlot = assignStringKeyInstruction->m_objectSlot;
                const char *key = assignStringKeyInstruction->m_key;
                if (info[objectSlot].m_staticObject && objectFastSlotsInitialized[objectSlot]) {
                    for (size_t k = 0; k < info[objectSlot].m_namesLength; k++) {
                        const char *name = info[objectSlot].m_namesData[k];
                        if (!strcmp(key, name)) {
                            const Slot fastSlot = fastSlots[objectSlot][k];
                            Gen::useRangeStart(&gen, instruction->m_belongsTo);
                            gen.m_scope = instruction->m_contextSlot;
                            Gen::addWriteFastSlot(&gen, assignStringKeyInstruction->m_valueSlot, fastSlot);
                            writes++;
                            Gen::useRangeEnd(&gen, instruction->m_belongsTo);
                            instruction = (Instruction *)(assignStringKeyInstruction + 1);
                            continue;
                        }
                    }
                }
            }
            Gen::addLike(&gen, instruction, Instruction::size(instruction), instruction);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }

    UserFunction *fn = Gen::buildFunction(&gen);
    const size_t newFastSlots = fn->m_fastSlots;
    copyFunctionStats(function, fn);
    fn->m_fastSlots = newFastSlots;

    u::Log::out("[script] => generated %zu fast slots (reads: %zu, writes: %zu)\n", defines, reads, writes);

    return fn;
}

// For access and assignments to primitive slots, inline the access/assignment with
// a static key instead
UserFunction *Optimize::inlinePass(UserFunction *function) {
    bool *primitiveSlots = nullptr;
    findPrimitiveSlots(function, &primitiveSlots);

    Gen gen = { };

    gen.m_slot = 1;
    gen.m_fastSlot = function->m_fastSlots;
    gen.m_blockTerminated = true;
    gen.m_currentRange = nullptr;

    size_t accesses = 0;
    size_t assignments = 0;
    size_t constraints = 0;

    u::vector<const char *> slotTable;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        Gen::newBlock(&gen);
        Instruction *instruction = InstructionBlock::begin(function, i);
        Instruction *instructionsEnd = InstructionBlock::end(function, i);
        while (instruction != instructionsEnd) {
            auto *newStringObject = (Instruction::NewStringObject *)instruction;
            auto *access = (Instruction::Access *)instruction;
            auto *assign = (Instruction::Assign *)instruction;
            auto *setConstraint = (Instruction::SetConstraint *)instruction;
            if (instruction->m_type == kNewStringObject
                && primitiveSlots[newStringObject->m_targetSlot])
            {
                if (slotTable.size() < newStringObject->m_targetSlot + 1)
                    slotTable.resize(newStringObject->m_targetSlot + 1);
                slotTable[newStringObject->m_targetSlot] = newStringObject->m_value;
                instruction = (Instruction *)(newStringObject + 1);
                continue;
            }
            if (instruction->m_type == kSetConstraint
                && setConstraint->m_keySlot < slotTable.size() && slotTable[setConstraint->m_keySlot])
            {
                Instruction::SetConstraintStringKey setConstraintStringKey;
                setConstraintStringKey.m_type = kSetConstraintStringKey;
                setConstraintStringKey.m_belongsTo = nullptr;
                setConstraintStringKey.m_objectSlot = setConstraint->m_objectSlot;
                setConstraintStringKey.m_constraintSlot = setConstraint->m_constraintSlot;
                setConstraintStringKey.m_key = slotTable[setConstraint->m_keySlot];
                setConstraintStringKey.m_keyLength = strlen(setConstraintStringKey.m_key);
                Gen::addLike(&gen, instruction, sizeof setConstraintStringKey, (Instruction *)&setConstraintStringKey);
                instruction = (Instruction *)(setConstraint + 1);
                constraints++;
                continue;
            }
            if (instruction->m_type == kAccess
                && access->m_keySlot < slotTable.size() && slotTable[access->m_keySlot])
            {
                Instruction::AccessStringKey accessStringKey;
                accessStringKey.m_type = kAccessStringKey;
                accessStringKey.m_objectSlot = access->m_objectSlot;
                accessStringKey.m_targetSlot = access->m_targetSlot;
                accessStringKey.m_key = slotTable[access->m_keySlot];
                Gen::addLike(&gen, instruction, sizeof accessStringKey, (Instruction *)&accessStringKey);
                instruction = (Instruction *)(access + 1);
                accesses++;
                continue;
            }
            if (instruction->m_type == kAssign
                && assign->m_keySlot < slotTable.size() && slotTable[assign->m_keySlot])
            {
                Instruction::AssignStringKey assignStringKey;
                assignStringKey.m_type = kAssignStringKey;
                assignStringKey.m_objectSlot = assign->m_objectSlot;
                assignStringKey.m_valueSlot = assign->m_valueSlot;
                assignStringKey.m_key = slotTable[assign->m_keySlot];
                assignStringKey.m_assignType = assign->m_assignType;
                Gen::addLike(&gen, instruction, sizeof assignStringKey, (Instruction *)&assignStringKey);
                instruction = (Instruction *)(assign + 1);
                assignments++;
                continue;
            }
            Gen::addLike(&gen, instruction, Instruction::size(instruction), instruction);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }
    UserFunction *optimized = Gen::buildFunction(&gen);
    copyFunctionStats(function, optimized);
    u::Log::out("[script] => inlined operations (assignments: %zu, accesses: %zu, constraints: %zu)\n", assignments, accesses, constraints);
    Memory::free(primitiveSlots);
    return optimized;
}

}
