#include "s_gen.h"
#include "s_memory.h"

#include "u_assert.h"
#include "u_vector.h"
#include "u_log.h"

namespace s {

BlockRef Gen::newBlockRef(Gen *gen, unsigned char *instruction, unsigned char *address) {
    FunctionBody *body = &gen->m_body;
    InstructionBlock *block = &body->m_blocks[body->m_count - 1];
    const size_t currentLength = (unsigned char *)block->m_instructionsEnd - (unsigned char *)block->m_instructions;
    const size_t delta = address - instruction;
    return { body->m_count - 1, currentLength + delta };
}

void Gen::setBlockRef(Gen *gen, BlockRef reference, size_t value) {
    FunctionBody *body = &gen->m_body;
    InstructionBlock *block = &body->m_blocks[reference.m_block];
    *(size_t *)((unsigned char *)block->m_instructions + reference.m_distance) = value;
}

void Gen::useRangeStart(Gen *gen, FileRange *range) {
    if (!gen) return;
    U_ASSERT(!gen->m_currentRange);
    gen->m_currentRange = range;
}

void Gen::useRangeEnd(Gen *gen, FileRange *range) {
    if (!gen) return;
    U_ASSERT(gen->m_currentRange == range);
    gen->m_currentRange = nullptr;
}

FileRange *Gen::newRange(char *text) {
    FileRange *range = (FileRange *)Memory::allocate(sizeof *range, 1);
    FileRange::recordStart(text, range);
    return range;
}

void Gen::delRange(FileRange *range) {
    Memory::free(range);
}

size_t Gen::newBlock(Gen *gen) {
    U_ASSERT(gen->m_blockTerminated);
    FunctionBody *body = &gen->m_body;
    body->m_blocks = (InstructionBlock *)Memory::reallocate(body->m_blocks, ++body->m_count * sizeof *body->m_blocks);
    body->m_blocks[body->m_count - 1] = { nullptr, nullptr };
    gen->m_blockTerminated = false;
    return body->m_count - 1;
}

void Gen::terminate(Gen *gen) {
    addReturn(gen, 0);
}

void Gen::addInstruction(Gen *gen, size_t size, Instruction *instruction) {
    U_ASSERT(Instruction::size(instruction) == size);
    U_ASSERT(!gen->m_blockTerminated);
    U_ASSERT(gen->m_currentRange);
    instruction->m_belongsTo = gen->m_currentRange;
    FunctionBody *body = &gen->m_body;
    InstructionBlock *block = &body->m_blocks[body->m_count - 1];
    const size_t currentLength = (unsigned char *)block->m_instructionsEnd - (unsigned char *)block->m_instructions;
    const size_t newLength = currentLength + size;
    block->m_instructions = (Instruction *)Memory::reallocate(block->m_instructions, newLength);
    block->m_instructionsEnd = (Instruction *)((unsigned char *)block->m_instructions + newLength);
    memcpy((unsigned char *)block->m_instructions + currentLength, instruction, size);
    if (instruction->m_type == kBranch || instruction->m_type == kTestBranch || instruction->m_type == kReturn)
        gen->m_blockTerminated = true;
}

Slot Gen::addAccess(Gen *gen, Slot objectSlot, Slot keySlot) {
    Instruction::Access access;
    access.m_type = kAccess;
    access.m_belongsTo = nullptr;
    access.m_objectSlot = objectSlot;
    access.m_keySlot = keySlot;
    access.m_targetSlot = gen->m_slot++;
    addInstruction(gen, sizeof access, (Instruction *)&access);
    return access.m_targetSlot;
}

void Gen::addAssign(Gen *gen, Slot objectSlot, Slot keySlot, Slot valueSlot, AssignType assignType) {
    Instruction::Assign assign;
    assign.m_type = kAssign;
    assign.m_belongsTo = nullptr;
    assign.m_objectSlot = objectSlot;
    assign.m_keySlot = keySlot;
    assign.m_valueSlot = valueSlot;
    assign.m_assignType = assignType;
    addInstruction(gen, sizeof assign, (Instruction *)&assign);
}

void Gen::addCloseObject(Gen *gen, Slot objectSlot) {
    Instruction::CloseObject closeObject;
    closeObject.m_type = kCloseObject;
    closeObject.m_slot = objectSlot;
    addInstruction(gen, sizeof closeObject, (Instruction *)&closeObject);
}

Slot Gen::addGetContext(Gen *gen) {
    Instruction::GetContext getContext;
    getContext.m_type = kGetContext;
    getContext.m_belongsTo = nullptr;
    getContext.m_slot = gen->m_slot++;
    addInstruction(gen, sizeof getContext, (Instruction *)&getContext);
    return gen->m_slot - 1;
}

Slot Gen::addNewObject(Gen *gen, Slot parentSlot) {
    Instruction::NewObject newObject;
    newObject.m_type = kNewObject;
    newObject.m_belongsTo = nullptr;
    newObject.m_targetSlot = gen->m_slot++;
    newObject.m_parentSlot = parentSlot;
    addInstruction(gen, sizeof newObject, (Instruction *)&newObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewIntObject(Gen *gen, Slot, int value) {
    Instruction::NewIntObject newIntObject;
    newIntObject.m_type = kNewIntObject;
    newIntObject.m_belongsTo = nullptr;
    newIntObject.m_targetSlot = gen->m_slot++;
    newIntObject.m_value = value;
    newIntObject.m_intObject = nullptr;
    addInstruction(gen, sizeof newIntObject, (Instruction *)&newIntObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewFloatObject(Gen *gen, Slot, float value) {
    Instruction::NewFloatObject newFloatObject;
    newFloatObject.m_type = kNewFloatObject;
    newFloatObject.m_belongsTo = nullptr;
    newFloatObject.m_targetSlot = gen->m_slot++;
    newFloatObject.m_value = value;
    newFloatObject.m_floatObject = nullptr;
    addInstruction(gen, sizeof newFloatObject, (Instruction *)&newFloatObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewArrayObject(Gen *gen, Slot) {
    Instruction::NewArrayObject newArrayObject;
    newArrayObject.m_type = kNewArrayObject;
    newArrayObject.m_belongsTo = nullptr;
    newArrayObject.m_targetSlot = gen->m_slot++;
    addInstruction(gen, sizeof newArrayObject, (Instruction *)&newArrayObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewStringObject(Gen *gen, Slot, const char *value) {
    Instruction::NewStringObject newStringObject;
    newStringObject.m_type = kNewStringObject;
    newStringObject.m_belongsTo = nullptr;
    newStringObject.m_targetSlot = gen->m_slot++;
    newStringObject.m_value = value;
    newStringObject.m_stringObject = nullptr;
    addInstruction(gen, sizeof newStringObject, (Instruction *)&newStringObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewClosureObject(Gen *gen, Slot contextSlot, UserFunction *function) {
    Instruction::NewClosureObject newClosureObject;
    newClosureObject.m_type = kNewClosureObject;
    newClosureObject.m_belongsTo = nullptr;
    newClosureObject.m_targetSlot = gen->m_slot++;
    newClosureObject.m_contextSlot = contextSlot;
    newClosureObject.m_function = function;
    addInstruction(gen, sizeof newClosureObject, (Instruction *)&newClosureObject);
    return gen->m_slot - 1;
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot *arguments, size_t count) {
    Instruction::Call call;
    call.m_type = kCall;
    call.m_belongsTo = nullptr;
    call.m_functionSlot = functionSlot;
    call.m_thisSlot = thisSlot;
    call.m_arguments = arguments;
    call.m_count = count;
    addInstruction(gen, sizeof call, (Instruction *)&call);

    Instruction::SaveResult saveResult;
    saveResult.m_type = kSaveResult;
    saveResult.m_targetSlot = gen->m_slot++;
    addInstruction(gen, sizeof saveResult, (Instruction *)&saveResult);

    return gen->m_slot - 1;
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot) {
    return addCall(gen, functionSlot, thisSlot, nullptr, 0);
}


Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0) {
    Slot *arguments = (Slot *)Memory::allocate(sizeof *arguments * 1);
    arguments[0] = argument0;
    return addCall(gen, functionSlot, thisSlot, arguments, 1);
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0, Slot argument1) {
    Slot *arguments = (Slot *)Memory::allocate(sizeof *arguments * 2);
    arguments[0] = argument0;
    arguments[1] = argument1;
    return addCall(gen, functionSlot, thisSlot, arguments, 2);
}

void Gen::addTestBranch(Gen *gen, Slot testSlot, BlockRef *trueBranch, BlockRef *falseBranch) {
    Instruction::TestBranch testBranch;
    testBranch.m_type = kTestBranch;
    testBranch.m_belongsTo = nullptr;
    testBranch.m_testSlot = testSlot;
    *trueBranch = newBlockRef(gen, (unsigned char *)&testBranch, (unsigned char *)&testBranch.m_trueBlock);
    *falseBranch = newBlockRef(gen, (unsigned char *)&testBranch, (unsigned char *)&testBranch.m_falseBlock);

    addInstruction(gen, sizeof testBranch, (Instruction *)&testBranch);
}

void Gen::addBranch(Gen *gen, BlockRef *branchBlock) {
    Instruction::Branch branch;
    branch.m_type = kBranch;
    branch.m_belongsTo = nullptr;
    *branchBlock = newBlockRef(gen, (unsigned char *)&branch, (unsigned char *)&branch.m_block);
    addInstruction(gen, sizeof branch, (Instruction *)&branch);
}

void Gen::addReturn(Gen *gen, Slot returnSlot) {
    Instruction::Return return_;
    return_.m_type = kReturn;
    return_.m_belongsTo = nullptr;
    return_.m_returnSlot = returnSlot;

    addInstruction(gen, sizeof return_, (Instruction *)&return_);
}

void Gen::addFreeze(Gen *gen, Slot object) {
    Instruction::Freeze freeze;
    freeze.m_type = kFreeze;
    freeze.m_belongsTo = nullptr;
    freeze.m_slot = object;
    addInstruction(gen, sizeof freeze, (Instruction *)&freeze);
}

Slot Gen::addDefineFastSlot(Gen *gen, Slot objectSlot, const char *key) {
    Instruction::DefineFastSlot defineFastSlot;
    const size_t keyLength = strlen(key);
    defineFastSlot.m_type = kDefineFastSlot;
    defineFastSlot.m_belongsTo = nullptr;
    defineFastSlot.m_objectSlot = objectSlot;
    defineFastSlot.m_key = key;
    defineFastSlot.m_keyLength = keyLength;
    defineFastSlot.m_keyHash = djb2(key, keyLength);
    defineFastSlot.m_targetSlot = gen->m_fastSlot++;
    addInstruction(gen, sizeof defineFastSlot, (Instruction *)&defineFastSlot);
    return defineFastSlot.m_targetSlot;
}

void Gen::addReadFastSlot(Gen *gen, Slot sourceSlot, Slot targetSlot) {
    Instruction::ReadFastSlot readFastSlot;
    readFastSlot.m_type = kReadFastSlot;
    readFastSlot.m_belongsTo = nullptr;
    readFastSlot.m_sourceSlot = sourceSlot;
    readFastSlot.m_targetSlot = targetSlot;
    addInstruction(gen, sizeof readFastSlot, (Instruction *)&readFastSlot);
}

void Gen::addWriteFastSlot(Gen *gen, Slot sourceSlot, Slot targetSlot) {
    Instruction::WriteFastSlot writeFastSlot;
    writeFastSlot.m_type = kWriteFastSlot;
    writeFastSlot.m_belongsTo = nullptr;
    writeFastSlot.m_sourceSlot = sourceSlot;
    writeFastSlot.m_targetSlot = targetSlot;
    addInstruction(gen, sizeof writeFastSlot, (Instruction *)&writeFastSlot);
}

UserFunction *Gen::buildFunction(Gen *gen) {
    U_ASSERT(gen->m_blockTerminated);
    UserFunction *function = (UserFunction *)Memory::allocate(sizeof *function);
    function->m_arity = gen->m_count;
    function->m_slots = gen->m_slot;
    function->m_fastSlots = gen->m_fastSlot;
    function->m_name = gen->m_name;
    function->m_body = gen->m_body;
    function->m_isMethod = false;
    function->m_hasVariadicTail = gen->m_hasVariadicTail;
    return function;
}

void Gen::copyFunctionStats(UserFunction *from, UserFunction *to) {
    to->m_slots = from->m_slots;
    to->m_fastSlots = from->m_fastSlots;
    to->m_arity = from->m_arity;
    to->m_name = from->m_name;
    to->m_isMethod = from->m_isMethod;
    to->m_hasVariadicTail = from->m_hasVariadicTail;
}

// Searches for static object slots which are any objects that are marked closed
// and any assignments are with static keys
struct SlotObjectInfo {
    bool m_staticObject;
    Slot m_parentSlot;
    const char **m_namesData;
    size_t m_namesLength;
    FileRange *m_belongsTo;
    Instruction *m_afterObjectDecl;
};

static inline void findStaticObjectSlots(UserFunction *function, SlotObjectInfo **slots) {
    *slots = (SlotObjectInfo *)Memory::allocate(sizeof **slots, function->m_slots);
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        const auto *block = &function->m_body.m_blocks[i];
        Instruction *instruction = block->m_instructions;
        Instruction *instructionEnd = block->m_instructionsEnd;
        while (instruction != instructionEnd) {
            if (instruction->m_type == kNewObject) {
                const auto *newObjectInstruction = (Instruction::NewObject *)instruction;
                instruction = (Instruction *)(newObjectInstruction + 1);
                bool failed = false;
                const char **namesData = nullptr;
                size_t namesLength = 0;
                while (instruction != instructionEnd && instruction->m_type == kAssignStringKey) {
                    const auto *assignStringKeyInstruction = (Instruction::AssignStringKey *)instruction;
                    if (assignStringKeyInstruction->m_assignType != kAssignPlain) {
                        failed = true;
                        break;
                    }
                    instruction = (Instruction *)(assignStringKeyInstruction + 1);
                    namesData = (const char **)Memory::reallocate(namesData, sizeof *namesData * ++namesLength);
                    namesData[namesLength - 1] = assignStringKeyInstruction->m_key;
                }
                if (instruction->m_type != kCloseObject)
                    failed = true;
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

                instruction = (Instruction *)((Instruction::CloseObject *)instruction + 1);
                (*slots)[targetSlot].m_afterObjectDecl = instruction;
                continue;
            }
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }
}

// Searches for slots which are primitive. Slots which are primitive are plain
// objects, calls, returns, branches and plain access and assignments.
static inline void findPrimitiveSlots(UserFunction *function, bool **slots) {
    *slots = (bool *)Memory::allocate(sizeof **slots * function->m_slots);
    for (size_t i = 0; i < function->m_slots; i++)
        (*slots)[i] = true;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        InstructionBlock *block = &function->m_body.m_blocks[i];
        Instruction *instruction = block->m_instructions;
        while (instruction != block->m_instructionsEnd) {
            switch (instruction->m_type) {
            case kNewObject:
                (*slots)[((Instruction::NewObject*)instruction)->m_parentSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::NewObject));
                break;
            case kNewClosureObject:
                (*slots)[((Instruction::NewClosureObject*)instruction)->m_contextSlot] = false;
                instruction = (Instruction *)((unsigned char *)instruction + sizeof(Instruction::NewClosureObject));
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

// For access and assignments to primitive slots, inline the access/assignment with
// a static key instead
UserFunction *Gen::inlinePass(UserFunction *function, bool *primitiveSlots) {
    Gen gen = { };

    gen.m_slot = 1;
    gen.m_fastSlot = function->m_fastSlots;
    gen.m_blockTerminated = true;
    gen.m_currentRange = nullptr;

    size_t count = 0;

    u::vector<const char *> slotTable;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        InstructionBlock *block = &function->m_body.m_blocks[i];
        newBlock(&gen);

        Instruction *instruction = block->m_instructions;
        while (instruction != block->m_instructionsEnd) {
            Instruction::NewStringObject *newStringObject = (Instruction::NewStringObject *)instruction;
            Instruction::Access *access = (Instruction::Access *)instruction;
            Instruction::Assign *assign = (Instruction::Assign *)instruction;
            if (instruction->m_type == kNewStringObject
                && primitiveSlots[newStringObject->m_targetSlot])
            {
                if (slotTable.size() < newStringObject->m_targetSlot + 1)
                    slotTable.resize(newStringObject->m_targetSlot + 1);
                slotTable[newStringObject->m_targetSlot] = newStringObject->m_value;
                instruction = (Instruction *)(newStringObject + 1);
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
                useRangeStart(&gen, access->m_belongsTo);
                addInstruction(&gen, sizeof accessStringKey, (Instruction *)&accessStringKey);
                useRangeEnd(&gen, access->m_belongsTo);
                instruction = (Instruction *)(access + 1);
                count++;
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
                useRangeStart(&gen, assign->m_belongsTo);
                addInstruction(&gen, sizeof assignStringKey, (Instruction *)&assignStringKey);
                useRangeEnd(&gen, assign->m_belongsTo);
                instruction = (Instruction *)(assign + 1);
                count++;
                continue;
            }
            useRangeStart(&gen, instruction->m_belongsTo);
            addInstruction(&gen, Instruction::size(instruction), instruction);
            useRangeEnd(&gen, instruction->m_belongsTo);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }
    UserFunction *optimized = buildFunction(&gen);
    copyFunctionStats(function, optimized);
    u::Log::out("Inlined Assignments/Accesses: %zu\n", count);
    return optimized;
}

UserFunction *Gen::predictPass(UserFunction *function) {
    // TODO: audit and fix?
    SlotObjectInfo *info = nullptr;
    findStaticObjectSlots(function, &info);

    size_t count = 0;

    Gen gen = { };
    gen.m_slot = 1;
    gen.m_fastSlot = function->m_fastSlots;
    gen.m_blockTerminated = true;

    for (size_t i = 0; i < function->m_body.m_count; i++) {
        const InstructionBlock *const block = &function->m_body.m_blocks[i];
        newBlock(&gen);

        Instruction *instruction = block->m_instructions;
        while (instruction != block->m_instructionsEnd) {
            const auto *accessStringKey = (Instruction::AccessStringKey *)instruction;
            if (instruction->m_type == kAccessStringKey) {
                Instruction::AccessStringKey newAccessStringKey = *accessStringKey;
                for (;;) {
                    const Slot objectSlot = newAccessStringKey.m_objectSlot;
                    if (!info[objectSlot].m_staticObject) {
                        break;
                    }
                    bool keyInObject = false;
                    for (size_t i = 0; i < info[objectSlot].m_namesLength; i++) {
                        const char *objectKey = info[objectSlot].m_namesData[i];
                        if (strcmp(objectKey, newAccessStringKey.m_key) == 0) {
                            keyInObject = true;
                            break;
                        }
                    }
                    if (keyInObject) {
                        break;
                    }
                    // So the key was not found in the object. We now know statically
                    // that the lookup will never succeed at runtime either.
                    // So automatically set the object slot to the parent. This
                    // allows us to skip searching up the object chain.
                    newAccessStringKey.m_objectSlot = info[objectSlot].m_parentSlot;
                    count++;
                }
                useRangeStart(&gen, instruction->m_belongsTo);
                addInstruction(&gen, sizeof newAccessStringKey, (Instruction *)&newAccessStringKey);
                useRangeEnd(&gen, instruction->m_belongsTo);
                instruction = (Instruction *)(accessStringKey + 1);
                continue;
            }

            useRangeStart(&gen, instruction->m_belongsTo);
            addInstruction(&gen, Instruction::size(instruction), instruction);
            useRangeEnd(&gen, instruction->m_belongsTo);

            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }

    Memory::free(info);

    UserFunction *optimized = buildFunction(&gen);
    copyFunctionStats(function, optimized);

    u::Log::out("Redirected predictable lookup misses: %zu\n", count);

    return optimized;
}

UserFunction *Gen::fastSlotPass(UserFunction *function) {
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
        InstructionBlock *block = &function->m_body.m_blocks[i];
        newBlock(&gen);

        Instruction *instruction = block->m_instructions;
        while (instruction != block->m_instructionsEnd) {
            for (size_t k = 0; k < infoSlotsLength; k++) {
                const Slot slot = infoSlots[k];
                if (instruction == info[slot].m_afterObjectDecl) {
                    useRangeStart(&gen, info[slot].m_belongsTo);
                    for (size_t l = 0; l < info[slot].m_namesLength; l++) {
                        fastSlots[slot][l] = addDefineFastSlot(&gen, slot, info[slot].m_namesData[l]);
                        defines++;
                    }
                    useRangeEnd(&gen, info[slot].m_belongsTo);
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
                            useRangeStart(&gen, instruction->m_belongsTo);
                            addReadFastSlot(&gen, fastSlot, accessStringKeyInstruction->m_targetSlot);
                            reads++;
                            useRangeEnd(&gen, instruction->m_belongsTo);
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
                            useRangeStart(&gen, instruction->m_belongsTo);
                            addWriteFastSlot(&gen, assignStringKeyInstruction->m_valueSlot, fastSlot);
                            writes++;
                            useRangeEnd(&gen, instruction->m_belongsTo);
                            instruction = (Instruction *)(assignStringKeyInstruction + 1);
                            continue;
                        }
                    }
                }
            }

            useRangeStart(&gen, instruction->m_belongsTo);
            addInstruction(&gen, Instruction::size(instruction), instruction);
            useRangeEnd(&gen, instruction->m_belongsTo);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }

    UserFunction *fn = buildFunction(&gen);
    const size_t newFastSlots = fn->m_fastSlots;
    copyFunctionStats(function, fn);
    fn->m_fastSlots = newFastSlots;

    u::Log::out("Generated %zu fast slots replacing %zu reads and %zu writes\n", defines, reads, writes);

    return fn;
}

UserFunction *Gen::optimize(UserFunction *function) {
    bool *primitiveSlots = nullptr;
    findPrimitiveSlots(function, &primitiveSlots);

    UserFunction *f0 = function;
    UserFunction *f1 = inlinePass(f0, primitiveSlots);
    UserFunction *f2 = predictPass(f1);
    UserFunction *f3 = fastSlotPass(f2);

    Memory::free(primitiveSlots);
    Memory::free(f0);
    Memory::free(f1);
    Memory::free(f2);

    return f3;
}

size_t Gen::scopeEnter(Gen *gen) {
    return gen->m_scope;
}

void Gen::scopeLeave(Gen *gen, size_t backup) {
    gen->m_scope = backup;
}

}
