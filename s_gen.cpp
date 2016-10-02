#include "s_gen.h"

#include "u_assert.h"
#include "u_vector.h"

namespace s {

size_t Gen::newBlock(Gen *gen) {
    U_ASSERT(gen->m_blockTerminated);
    FunctionBody *body = &gen->m_body;
    body->m_blocks = (InstructionBlock *)neoRealloc(body->m_blocks, ++body->m_count * sizeof *body->m_blocks);
    body->m_blocks[body->m_count - 1] = { nullptr, 0 };
    gen->m_blockTerminated = false;
    return body->m_count - 1;
}

void Gen::terminate(Gen *gen) {
    addReturn(gen, gen->m_slot++);
}

void Gen::addInstruction(Gen *gen, Instruction *instruction) {
    U_ASSERT(!gen->m_blockTerminated);
    FunctionBody *body = &gen->m_body;
    InstructionBlock *block = &body->m_blocks[body->m_count - 1];
    block->m_instructions = (Instruction **)neoRealloc(block->m_instructions, ++block->m_count * sizeof *block->m_instructions);
    block->m_instructions[block->m_count - 1] = instruction;
    if (instruction->m_type == kBranch || instruction->m_type == kTestBranch || instruction->m_type == kReturn)
        gen->m_blockTerminated = true;
}

Slot Gen::addAccess(Gen *gen, Slot objectSlot, Slot keySlot) {
    Instruction::Access *access = (Instruction::Access *)neoMalloc(sizeof *access);
    access->m_type = kAccess;
    access->m_objectSlot = objectSlot;
    access->m_keySlot = keySlot;
    access->m_targetSlot = gen->m_slot++;
    addInstruction(gen, (Instruction *)access);
    return access->m_targetSlot;
}

void Gen::addAssign(Gen *gen, Slot objectSlot, Slot keySlot, Slot valueSlot, AssignType assignType) {
    Instruction::Assign *assign = (Instruction::Assign *)neoMalloc(sizeof *assign);
    assign->m_type = kAssign;
    assign->m_objectSlot = objectSlot;
    assign->m_keySlot = keySlot;
    assign->m_valueSlot = valueSlot;
    assign->m_assignType = assignType;
    addInstruction(gen, (Instruction *)assign);
}

void Gen::addCloseObject(Gen *gen, Slot objectSlot) {
    Instruction::CloseObject *closeObject = (Instruction::CloseObject *)neoMalloc(sizeof *closeObject);
    closeObject->m_type = kCloseObject;
    closeObject->m_slot = objectSlot;
    addInstruction(gen, (Instruction *)closeObject);
}

Slot Gen::addGetContext(Gen *gen) {
    Instruction::GetContext *getContext = (Instruction::GetContext *)neoMalloc(sizeof *getContext);
    getContext->m_type = kGetContext;
    getContext->m_slot = gen->m_slot++;
    addInstruction(gen, (Instruction *)getContext);
    return gen->m_slot - 1;
}

Slot Gen::addNewObject(Gen *gen, Slot parentSlot) {
    Instruction::NewObject *newObject = (Instruction::NewObject *)neoMalloc(sizeof *newObject);
    newObject->m_type = kNewObject;
    newObject->m_targetSlot = gen->m_slot++;
    newObject->m_parentSlot = parentSlot;
    addInstruction(gen, (Instruction *)newObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewIntObject(Gen *gen, Slot, int value) {
    Instruction::NewIntObject *newIntObject = (Instruction::NewIntObject *)neoMalloc(sizeof *newIntObject);
    newIntObject->m_type = kNewIntObject;
    newIntObject->m_targetSlot = gen->m_slot++;
    newIntObject->m_value = value;
    addInstruction(gen, (Instruction *)newIntObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewFloatObject(Gen *gen, Slot, float value) {
    Instruction::NewFloatObject *newFloatObject = (Instruction::NewFloatObject *)neoMalloc(sizeof *newFloatObject);
    newFloatObject->m_type = kNewFloatObject;
    newFloatObject->m_targetSlot = gen->m_slot++;
    newFloatObject->m_value = value;
    addInstruction(gen, (Instruction *)newFloatObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewArrayObject(Gen *gen, Slot) {
    Instruction::NewArrayObject *newArrayObject = (Instruction::NewArrayObject *)neoMalloc(sizeof *newArrayObject);
    newArrayObject->m_type = kNewArrayObject;
    newArrayObject->m_targetSlot = gen->m_slot++;
    addInstruction(gen, (Instruction *)newArrayObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewStringObject(Gen *gen, Slot, const char *value) {
    Instruction::NewStringObject *newStringObject = (Instruction::NewStringObject *)neoMalloc(sizeof *newStringObject);
    newStringObject->m_type = kNewStringObject;
    newStringObject->m_targetSlot = gen->m_slot++;
    newStringObject->m_value = (char *)value; // TODO(daleweiler): fix
    addInstruction(gen, (Instruction *)newStringObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewClosureObject(Gen *gen, Slot contextSlot, UserFunction *function) {
    Instruction::NewClosureObject *newClosureObject = (Instruction::NewClosureObject *)neoMalloc(sizeof *newClosureObject);
    newClosureObject->m_type = kNewClosureObject;
    newClosureObject->m_targetSlot = gen->m_slot++;
    newClosureObject->m_contextSlot = contextSlot;
    newClosureObject->m_function = function;
    addInstruction(gen, (Instruction *)newClosureObject);
    return gen->m_slot - 1;
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot *arguments, size_t count) {
    Instruction::Call *call = (Instruction::Call *)neoMalloc(sizeof *call);
    call->m_type = kCall;
    call->m_functionSlot = functionSlot;
    call->m_thisSlot = thisSlot;
    call->m_arguments = arguments;
    call->m_count = count;
    addInstruction(gen, (Instruction *)call);

    Instruction::SaveResult *saveResult = (Instruction::SaveResult *)neoMalloc(sizeof *saveResult);
    saveResult->m_type = kSaveResult;
    saveResult->m_targetSlot = gen->m_slot++;
    addInstruction(gen, (Instruction *)saveResult);

    return gen->m_slot - 1;
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot) {
    return addCall(gen, functionSlot, thisSlot, nullptr, 0);
}


Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 1);
    arguments[0] = argument0;
    return addCall(gen, functionSlot, thisSlot, arguments, 1);
}

Slot Gen::addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0, Slot argument1) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 2);
    arguments[0] = argument0;
    arguments[1] = argument1;
    return addCall(gen, functionSlot, thisSlot, arguments, 2);
}

void Gen::addTestBranch(Gen *gen, Slot testSlot, size_t **trueBranch, size_t **falseBranch) {
    Instruction::TestBranch *testBranch = (Instruction::TestBranch *)neoMalloc(sizeof *testBranch);
    testBranch->m_type = kTestBranch;
    testBranch->m_testSlot = testSlot;
    *trueBranch = &testBranch->m_trueBlock;
    *falseBranch = &testBranch->m_falseBlock;

    addInstruction(gen, (Instruction *)testBranch);
}

void Gen::addBranch(Gen *gen, size_t **branchBlock) {
    Instruction::Branch *branch = (Instruction::Branch *)neoMalloc(sizeof *branch);
    branch->m_type = kBranch;
    *branchBlock = &branch->m_block;

    addInstruction(gen, (Instruction *)branch);
}

void Gen::addReturn(Gen *gen, Slot returnSlot) {
    Instruction::Return *return_ = (Instruction::Return *)neoMalloc(sizeof *return_);
    return_->m_type = kReturn;
    return_->m_returnSlot = returnSlot;

    addInstruction(gen, (Instruction *)return_);
}

UserFunction *Gen::buildFunction(Gen *gen) {
    U_ASSERT(gen->m_blockTerminated);
    UserFunction *function = (UserFunction *)neoMalloc(sizeof *function);
    function->m_arity = gen->m_count;
    function->m_slots = gen->m_slot;
    function->m_name = gen->m_name;
    function->m_body = gen->m_body;
    function->m_isMethod = false;
    return function;
}

static inline void findPrimitiveSlots(UserFunction *function, bool **slots) {
    *slots = (bool *)neoMalloc(sizeof **slots * function->m_slots);
    for (size_t i = 0; i < function->m_slots; i++)
        (*slots)[i] = true;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        InstructionBlock *block = &function->m_body.m_blocks[i];
        for (size_t k = 0; k < block->m_count; k++) {
            Instruction *instruction = block->m_instructions[k];
            switch (instruction->m_type) {
            case kNewObject:
                (*slots)[((Instruction::NewObject*)instruction)->m_parentSlot] = false;
                break;
            case kNewClosureObject:
                (*slots)[((Instruction::NewClosureObject*)instruction)->m_contextSlot] = false;
                break;
            case kAccess:
                (*slots)[((Instruction::Access*)instruction)->m_objectSlot] = false;
                break;
            case kAssign:
                (*slots)[((Instruction::Assign*)instruction)->m_objectSlot] = false;
                (*slots)[((Instruction::Assign*)instruction)->m_valueSlot] = false;
                break;
            case kCall:
                (*slots)[((Instruction::Call*)instruction)->m_functionSlot] = false;
                (*slots)[((Instruction::Call*)instruction)->m_thisSlot] = false;
                for (size_t i = 0; i < ((Instruction::Call*)instruction)->m_count; i++)
                    (*slots)[((Instruction::Call*)instruction)->m_arguments[i]] = false;
                break;
            case kReturn:
                (*slots)[((Instruction::Return*)instruction)->m_returnSlot] = false;
                break;
            case kTestBranch:
                (*slots)[((Instruction::TestBranch*)instruction)->m_testSlot] = false;
                break;
            default:
                break;
            }
        }
    }
}

UserFunction *Gen::optimize(UserFunction *function) {
    bool *primitiveSlots = nullptr;
    findPrimitiveSlots(function, &primitiveSlots);
    UserFunction *optimized = inlinePass(function, primitiveSlots);
    neoFree(primitiveSlots);
    neoFree(function);
    return optimized;
}

UserFunction *Gen::inlinePass(UserFunction *function, bool *primitiveSlots) {
    Gen *gen = (Gen *)neoCalloc(sizeof *gen, 1);
    gen->m_slot = 0;
    gen->m_blockTerminated = true;
    u::vector<char *> slotTable;
    for (size_t i = 0; i < function->m_body.m_count; i++) {
        InstructionBlock *block = &function->m_body.m_blocks[i];
        newBlock(gen);
        for (size_t k = 0; k < block->m_count; k++) {
            Instruction *instruction = block->m_instructions[k];
            Instruction::NewStringObject *newStringObject = (Instruction::NewStringObject *)instruction;
            Instruction::Access *access = (Instruction::Access *)instruction;
            Instruction::Assign *assign = (Instruction::Assign *)instruction;
            if (instruction->m_type == kNewStringObject
                && primitiveSlots[newStringObject->m_targetSlot])
            {
                if (slotTable.size() < newStringObject->m_targetSlot + 1)
                    slotTable.resize(newStringObject->m_targetSlot + 1);
                slotTable[newStringObject->m_targetSlot] = newStringObject->m_value;
                continue;
            }
            if (instruction->m_type == kAccess
                && access->m_keySlot < slotTable.size() && slotTable[access->m_keySlot])
            {
                Instruction::AccessStringKey *accessStringKey =
                    (Instruction::AccessStringKey *)neoMalloc(sizeof *accessStringKey);
                accessStringKey->m_type = kAccesStringKey;
                accessStringKey->m_objectSlot = access->m_objectSlot;
                accessStringKey->m_targetSlot = access->m_targetSlot;
                accessStringKey->m_key = slotTable[access->m_keySlot];
                addInstruction(gen, (Instruction *)accessStringKey);
                continue;
            }
            if (instruction->m_type == kAssign
                && assign->m_keySlot < slotTable.size() && slotTable[assign->m_keySlot])
            {
                Instruction::AssignStringKey *assignStringKey =
                    (Instruction::AssignStringKey *)neoMalloc(sizeof *assignStringKey);
                assignStringKey->m_type = kAssignStringKey;
                assignStringKey->m_objectSlot = assign->m_objectSlot;
                assignStringKey->m_valueSlot = assign->m_valueSlot;
                assignStringKey->m_key = slotTable[assign->m_keySlot];
                assignStringKey->m_assignType = assign->m_assignType;
                addInstruction(gen, (Instruction *)assignStringKey);
                continue;
            }
            addInstruction(gen, instruction);
        }
    }

    UserFunction *optimized = buildFunction(gen);
    optimized->m_slots = function->m_slots;
    optimized->m_arity = function->m_arity;
    optimized->m_name = function->m_name;
    optimized->m_isMethod = function->m_isMethod;
    neoFree(gen);

    return optimized;
}

}
