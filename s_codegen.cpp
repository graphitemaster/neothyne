#include <string.h>

#include "s_object.h"
#include "s_codegen.h"

#include "u_assert.h"

namespace s {

Block FunctionCodegen::newBlock() {
    U_ASSERT(m_terminated);
    FunctionBody *body = &m_body;
    body->m_length++;
    body->m_blocks = (InstrBlock *)neoRealloc(body->m_blocks, body->m_length * sizeof(InstrBlock));
    body->m_blocks[body->m_length - 1].m_instrs = nullptr;
    body->m_blocks[body->m_length - 1].m_length = 0;
    m_terminated = false;
    return body->m_length - 1;
}

void FunctionCodegen::terminate() {
    // return null
    addReturn(m_slotBase++);
}

void FunctionCodegen::addInstr(Instr *instruction) {
    U_ASSERT(!m_terminated);
    FunctionBody *body = &m_body;
    InstrBlock *block = &body->m_blocks[body->m_length - 1];
    block->m_length++;
    block->m_instrs = (Instr **)neoRealloc(block->m_instrs, block->m_length * sizeof(Instr *));
    block->m_instrs[block->m_length - 1] = instruction;
}

Slot FunctionCodegen::addAccess(Slot objectSlot, Slot keySlot) {
    auto *instruction = allocate<AccessInstr>();
    instruction->m_type = Instr::kAccess;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_objectSlot = objectSlot;
    instruction->m_keySlot = keySlot;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

void FunctionCodegen::addAssignNormal(Slot object, Slot keySlot, Slot slot) {
    auto *instruction = allocate<AssignNormalInstr>();
    instruction->m_type = Instr::kAssignNormal;
    instruction->m_objectSlot = object;
    instruction->m_valueSlot = slot;
    instruction->m_keySlot = keySlot;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addAssignExisting(Slot object, Slot keySlot, Slot slot) {
    auto *instruction = allocate<AssignExistingInstr>();
    instruction->m_type = Instr::kAssignExisting;
    instruction->m_objectSlot = object;
    instruction->m_valueSlot = slot;
    instruction->m_keySlot = keySlot;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addAssignShadowing(Slot object, Slot keySlot, Slot slot) {
    auto *instruction = allocate<AssignShadowingInstr>();
    instruction->m_type = Instr::kAssignShadowing;
    instruction->m_objectSlot = object;
    instruction->m_valueSlot = slot;
    instruction->m_keySlot = keySlot;
    addInstr((Instr *)instruction);
}

void FunctionCodegen::addCloseObject(Slot object) {
    auto *instruction = allocate<CloseObjectInstr>();
    instruction->m_type = Instr::kCloseObject;
    instruction->m_slot = object;
    addInstr((Instr *)instruction);
}

Slot FunctionCodegen::addGetContext() {
    auto *instruction = allocate<GetContextInstr>();
    instruction->m_type = Instr::kGetContext;
    instruction->m_slot = m_slotBase++;
    addInstr((Instr *)instruction);
    return instruction->m_slot;
}

Slot FunctionCodegen::addAllocObject(Slot parent) {
    auto *instruction = allocate<AllocObjectInstr>();
    instruction->m_type = Instr::kAllocObject;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_parentSlot = parent;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addAllocClosureObject(Slot contextSlot, UserFunction *function) {
    auto *instruction = allocate<AllocClosureObjectInstr>();
    instruction->m_type = Instr::kAllocClosureObject;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_contextSlot = contextSlot;
    instruction->m_function = function;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addAllocIntObject(Slot contextSlot, int value) {
    auto *instruction = allocate<AllocIntObjectInstr>();
    instruction->m_type = Instr::kAllocIntObject;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addAllocFloatObject(Slot contextSlot, float value) {
    auto *instruction = allocate<AllocFloatObjectInstr>();
    instruction->m_type = Instr::kAllocFloatObject;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addAllocStringObject(Slot contextSlot, const char *value) {
    auto *instruction = allocate<AllocStringObjectInstr>();
    instruction->m_type = Instr::kAllocStringObject;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addCall(Slot function, Slot thisSlot, Slot *arguments, size_t length) {
    auto *instruction = allocate<CallInstr>();
    instruction->m_type = Instr::kCall;
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_functionSlot = function;
    instruction->m_thisSlot = thisSlot;
    instruction->m_length = length;
    instruction->m_arguments = arguments;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot FunctionCodegen::addCall(Slot function, Slot thisSlot) {
    return addCall(function, thisSlot, nullptr, 0);
}

Slot FunctionCodegen::addCall(Slot function, Slot thisSlot, Slot arg0) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 1);
    arguments[0] = arg0;
    return addCall(function, thisSlot, arguments, 1);
}

Slot FunctionCodegen::addCall(Slot function, Slot thisSlot, Slot arg0, Slot arg1) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 2);
    arguments[0] = arg0;
    arguments[1] = arg1;
    return addCall(function, thisSlot, arguments, 2);
}

void FunctionCodegen::addTestBranch(Slot test, Block **trueBranch, Block **falseBranch) {
    auto *instruction = allocate<TestBranchInstr>();
    instruction->m_type = Instr::kTestBranch;
    instruction->m_testSlot = test;
    *trueBranch = &instruction->m_trueBlock;
    *falseBranch = &instruction->m_falseBlock;
    addInstr((Instr *)instruction);
    m_terminated = true;
}

void FunctionCodegen::addBranch(Block **branch) {
    auto *instruction = allocate<BranchInstr>();
    instruction->m_type = Instr::kBranch;
    *branch = &instruction->m_block;
    addInstr((Instr *)instruction);
    m_terminated = true;
}

void FunctionCodegen::addReturn(Slot slot) {
    auto *instruction = allocate<ReturnInstr>();
    instruction->m_type = Instr::kReturn;
    instruction->m_returnSlot = slot;
    addInstr((Instr *)instruction);
    m_terminated = true;
}

UserFunction *FunctionCodegen::build() {
    U_ASSERT(m_terminated);
    auto *function = allocate<UserFunction>();
    function->m_arity = m_length;
    function->m_slots = m_slotBase;
    function->m_name = m_name;
    function->m_body = m_body;
    return function;
}

}
