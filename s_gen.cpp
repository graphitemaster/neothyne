#include "s_gen.h"
#include "s_memory.h"
#include "s_optimize.h"

#include "u_assert.h"

namespace s {

BlockRef Gen::newBlockRef(Gen *gen, unsigned char *instruction, unsigned char *address) {
    FunctionBody *body = &gen->m_body;
    const size_t currentLength = (unsigned char *)body->m_instructionsEnd - (unsigned char *)body->m_instructions;
    const size_t delta = address - instruction;
    return currentLength + delta;
}

void Gen::setBlockRef(Gen *gen, BlockRef offset, size_t value) {
    FunctionBody *body = &gen->m_body;
    *(size_t *)((unsigned char *)body->m_instructions + offset) = value;
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
    const size_t offset = (unsigned char *)body->m_instructionsEnd - (unsigned char *)body->m_instructions;
    body->m_blocks = (InstructionBlock *)Memory::reallocate(body->m_blocks, ++body->m_count * sizeof *body->m_blocks);
    body->m_blocks[body->m_count - 1] = { offset, 0 };
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
    instruction->m_contextSlot = gen->m_scope;
    FunctionBody *body = &gen->m_body;
    InstructionBlock *block = &body->m_blocks[body->m_count - 1];
    const size_t currentLength = (unsigned char *)body->m_instructionsEnd - (unsigned char *)body->m_instructions;
    const size_t newLength = currentLength + size;
    body->m_instructions = (Instruction *)Memory::reallocate(body->m_instructions, newLength);
    body->m_instructionsEnd = (Instruction *)((unsigned char *)body->m_instructions + newLength);
    block->m_size += size;
    memcpy((unsigned char *)body->m_instructions + currentLength, instruction, size);
    if (instruction->m_type == kBranch || instruction->m_type == kTestBranch || instruction->m_type == kReturn)
        gen->m_blockTerminated = true;
}

void Gen::addLike(Gen *gen, Instruction *basis, size_t size, Instruction *instruction) {
    const Slot backup = Gen::scopeEnter(gen);
    useRangeStart(gen, basis->m_belongsTo);
    scopeSet(gen, basis->m_contextSlot);
    addInstruction(gen, size, instruction);
    useRangeEnd(gen, basis->m_belongsTo);
    scopeLeave(gen, backup);
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
    closeObject.m_belongsTo = nullptr;
    closeObject.m_slot = objectSlot;
    addInstruction(gen, sizeof closeObject, (Instruction *)&closeObject);
}

void Gen::addSetConstraint(Gen *gen, Slot objectSlot, Slot keySlot, Slot constraintSlot) {
    Instruction::SetConstraint setConstraint;
    setConstraint.m_type = kSetConstraint;
    setConstraint.m_belongsTo = nullptr;
    setConstraint.m_objectSlot = objectSlot;
    setConstraint.m_keySlot = keySlot;
    setConstraint.m_constraintSlot = constraintSlot;
    addInstruction(gen, sizeof setConstraint, (Instruction *)&setConstraint);
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

Slot Gen::addNewIntObject(Gen *gen, int value) {
    Instruction::NewIntObject newIntObject;
    newIntObject.m_type = kNewIntObject;
    newIntObject.m_belongsTo = nullptr;
    newIntObject.m_targetSlot = gen->m_slot++;
    newIntObject.m_value = value;
    newIntObject.m_intObject = nullptr;
    addInstruction(gen, sizeof newIntObject, (Instruction *)&newIntObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewFloatObject(Gen *gen, float value) {
    Instruction::NewFloatObject newFloatObject;
    newFloatObject.m_type = kNewFloatObject;
    newFloatObject.m_belongsTo = nullptr;
    newFloatObject.m_targetSlot = gen->m_slot++;
    newFloatObject.m_value = value;
    newFloatObject.m_floatObject = nullptr;
    addInstruction(gen, sizeof newFloatObject, (Instruction *)&newFloatObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewArrayObject(Gen *gen) {
    Instruction::NewArrayObject newArrayObject;
    newArrayObject.m_type = kNewArrayObject;
    newArrayObject.m_belongsTo = nullptr;
    newArrayObject.m_targetSlot = gen->m_slot++;
    addInstruction(gen, sizeof newArrayObject, (Instruction *)&newArrayObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewStringObject(Gen *gen, const char *value) {
    Instruction::NewStringObject newStringObject;
    newStringObject.m_type = kNewStringObject;
    newStringObject.m_belongsTo = nullptr;
    newStringObject.m_targetSlot = gen->m_slot++;
    newStringObject.m_value = value;
    newStringObject.m_stringObject = nullptr;
    addInstruction(gen, sizeof newStringObject, (Instruction *)&newStringObject);
    return gen->m_slot - 1;
}

Slot Gen::addNewClosureObject(Gen *gen, UserFunction *function) {
    Instruction::NewClosureObject newClosureObject;
    newClosureObject.m_type = kNewClosureObject;
    newClosureObject.m_belongsTo = nullptr;
    newClosureObject.m_targetSlot = gen->m_slot++;
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

UserFunction *Gen::optimize(UserFunction *f0) {
    UserFunction *f1 = Optimize::inlinePass(f0);
    UserFunction *f2 = Optimize::predictPass(f1);
    UserFunction *f3 = Optimize::fastSlotPass(f2);
    Memory::free(f0);
    Memory::free(f1);
    Memory::free(f2);
    return f3;
}

Slot Gen::scopeEnter(Gen *gen) {
    gen->m_lastScope = gen->m_scope;
    return gen->m_scope;
}

void Gen::scopeLeave(Gen *gen, Slot backup) {
    scopeSet(gen, backup);
}

void Gen::scopeSet(Gen *gen, Slot scope) {
    gen->m_scope = scope;
}

}
