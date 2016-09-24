#include <string.h>

#include "s_object.h"
#include "s_codegen.h"

namespace s {

FunctionCodegen::FunctionCodegen() {
    memset(this, 0, sizeof *this);
}

size_t FunctionCodegen::newBlock() {
    FunctionBody *body = &m_body;
    body->m_length++;
    body->m_blocks = (InstrBlock *)neoRealloc(body->m_blocks, body->m_length * sizeof(InstrBlock));
    body->m_blocks[body->m_length - 1] = { };
    return body->m_length - 1;
}

void FunctionCodegen::addInstr(Instr *instruction) {
    FunctionBody *body = &m_body;
    InstrBlock *block = &body->m_blocks[body->m_length - 1];
    block->m_length++;
    block->m_instrs = (Instr **)neoRealloc(block->m_instrs, block->m_length * sizeof(Instr *));
    block->m_instrs[block->m_length - 1] = instruction;
}

Slot FunctionCodegen::addAccess(Slot objectSlot, const char *identifier) {
    auto *instruction = allocate<AccessInstr>();
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_objectSlot = objectSlot;
    instruction->m_key = identifier;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

void FunctionCodegen::addAssign(Slot object, const char *name, Slot slot) {
    auto *instruction = allocate<AssignInstr>();
    instruction->m_objectSlot = object;
    instruction->m_valueSlot = slot;
    instruction->m_key = name;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addCloseObject(Slot object) {
    auto *instruction = allocate<CloseObjectInstr>();
    instruction->m_slot = object;
    addInstr((Instr *)instruction);
}

Slot FunctionCodegen::addGetContext() {
    auto *instruction = allocate<GetContextInstr>();
    instruction->m_slot = m_slotBase++;
    addInstr((Instr *)instruction);
    return instruction->m_slot;
}

Slot FunctionCodegen::addAllocObject(Slot parent) {
    auto *instruction = allocate<AllocObjectInstr>();
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_parentSlot = parent;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addAllocIntObject(int value) {
    auto *instruction = allocate<AllocIntObjectInstr>();
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addCall(Slot function, size_t *args, size_t length) {
    auto *instruction = allocate<CallInstr>();
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_functionSlot = function;
    instruction->m_argsLength = length;
    instruction->m_argsData = args;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

void FunctionCodegen::addTestBranch(Slot test, size_t **trueBranch, size_t **falseBranch) {
    auto *instruction = allocate<TestBranchInstr>();
    instruction->m_testSlot = test;
    *trueBranch = &instruction->m_trueBlock;
    *falseBranch = &instruction->m_falseBlock;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addBranch(size_t **branch) {
    auto *instruction = allocate<BranchInstr>();
    *branch = &instruction->m_block;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addReturn(Slot slot) {
    auto *instruction = allocate<ReturnInstr>();
    instruction->m_returnSlot = slot;
    addInstr((Instr *)instruction);
}

UserFunction *FunctionCodegen::build() {
    auto *function = allocate<UserFunction>();
    // TODO(daleweiler): figure out arity and slots for a function
    function->m_name = m_name;
    function->m_body = m_body;
    return function;
}

void FunctionCodegen::setName(const char *name) {
    m_name = name;
}

}
