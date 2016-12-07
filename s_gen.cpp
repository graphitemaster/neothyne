#include "s_gen.h"
#include "s_memory.h"

#include "u_assert.h"
#include "u_vector.h"

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

FileRange *Gen::newRange(Gen *gen, char *text) {
    if (!gen) return nullptr;
    FileRange *range = (FileRange *)Memory::allocate(gen->m_memory, sizeof *range, 1);
    FileRange::recordStart(text, range);
    return range;
}

void Gen::delRange(Gen *gen, FileRange *range) {
    if (!gen) return;
    Memory::free(gen->m_memory, range);
}

size_t Gen::newBlock(Gen *gen) {
    U_ASSERT(gen->m_blockTerminated);
    FunctionBody *body = &gen->m_body;
    body->m_blocks = (InstructionBlock *)Memory::reallocate(gen->m_memory, body->m_blocks, ++body->m_count * sizeof *body->m_blocks);
    body->m_blocks[body->m_count - 1] = { nullptr, nullptr };
    gen->m_blockTerminated = false;
    return body->m_count - 1;
}

void Gen::terminate(Gen *gen) {
    addReturn(gen, gen->m_slot++);
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
    block->m_instructions = (Instruction *)Memory::reallocate(gen->m_memory, block->m_instructions, newLength);
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
    Slot *arguments = (Slot *)Memory::allocate(gen->m_memory, sizeof *arguments * 1);
    arguments[0] = argument0;
    return addCall(gen, functionSlot, thisSlot, arguments, 1);
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0, Slot argument1) {
    Slot *arguments = (Slot *)Memory::allocate(gen->m_memory, sizeof *arguments * 2);
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

UserFunction *Gen::buildFunction(Gen *gen) {
    // NOTE: this should not use gen->m_memory since functions are garbage
    //       collected instead
    U_ASSERT(gen->m_blockTerminated);
    UserFunction *function = (UserFunction *)neoMalloc(sizeof *function);
    function->m_arity = gen->m_count;
    function->m_slots = gen->m_slot;
    function->m_name = gen->m_name;
    function->m_body = gen->m_body;
    function->m_isMethod = false;
    return function;
}

static inline void findPrimitiveSlots(Memory *memory, UserFunction *function, bool **slots) {
    *slots = (bool *)Memory::allocate(memory, sizeof **slots * function->m_slots);
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

UserFunction *Gen::optimize(Gen *gen, UserFunction *function) {
    bool *primitiveSlots = nullptr;
    findPrimitiveSlots(gen->m_memory, function, &primitiveSlots);
    UserFunction *optimized = inlinePass(gen->m_memory, function, primitiveSlots);
    Memory::free(gen->m_memory, primitiveSlots);
    // NOTE: function is garbage collected at runtime
    neoFree(function);
    return optimized;
}

UserFunction *Gen::inlinePass(Memory *memory, UserFunction *function, bool *primitiveSlots) {
    Gen gen = { };
    gen.m_memory = memory;
    gen.m_slot = 0;
    gen.m_blockTerminated = true;
    gen.m_currentRange = nullptr;
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
                continue;
            }
            useRangeStart(&gen, instruction->m_belongsTo);
            addInstruction(&gen, Instruction::size(instruction), instruction);
            useRangeEnd(&gen, instruction->m_belongsTo);
            instruction = (Instruction *)((unsigned char *)instruction + Instruction::size(instruction));
        }
    }

    UserFunction *optimized = buildFunction(&gen);
    optimized->m_slots = function->m_slots;
    optimized->m_arity = function->m_arity;
    optimized->m_name = function->m_name;
    optimized->m_isMethod = function->m_isMethod;

    return optimized;
}

size_t Gen::scopeEnter(Gen *gen) {
    return gen->m_scope;
}

void Gen::scopeLeave(Gen *gen, size_t backup) {
    gen->m_scope = backup;
}

}
