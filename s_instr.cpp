#include "s_instr.h"

namespace s {

Instr::Instr(int type)
    : m_type(type)
{
}

GetRootInstr::GetRootInstr(Slot slot)
    : Instr(kGetRoot)
    , m_slot(slot)
{
}

GetContextInstr::GetContextInstr(Slot slot)
    : Instr(kGetContext)
    , m_slot(slot)
{
}

AllocObjectInstr::AllocObjectInstr(Slot target, Slot parent)
    : Instr(kAllocObject)
    , m_targetSlot(target)
    , m_parentSlot(parent)
{
}

AllocIntObjectInstr::AllocIntObjectInstr(Slot target, int value)
    : Instr(kAllocIntObject)
    , m_targetSlot(target)
    , m_value(value)
{
}

CloseObjectInstr::CloseObjectInstr(Slot slot)
    : Instr(kCloseObject)
    , m_slot(slot)
{
}

AccessInstr::AccessInstr(Slot target, Slot object, const char *key)
    : Instr(kAccess)
    , m_targetSlot(target)
    , m_objectSlot(object)
    , m_key(key)
{
}

AssignInstr::AssignInstr(Slot object, Slot value, const char *key)
    : Instr(kAssign)
    , m_objectSlot(object)
    , m_valueSlot(value)
    , m_key(key)
{
}

CallInstr::CallInstr(Slot target, Slot function, Slot *arguments, size_t length)
    : Instr(kCall)
    , m_targetSlot(target)
    , m_functionSlot(function)
    , m_arguments(arguments)
    , m_length(length)
{
}


ReturnInstr::ReturnInstr(Slot slot)
    : Instr(kReturn)
    , m_returnSlot(slot)
{
}

BranchInstr::BranchInstr(size_t block)
    : Instr(kBranch)
    , m_block(block)
{
}

TestBranchInstr::TestBranchInstr(Slot testSlot, size_t trueBlock, size_t falseBlock)
    : Instr(kTestBranch)
    , m_testSlot(testSlot)
    , m_trueBlock(trueBlock)
    , m_falseBlock(falseBlock)
{
}

}
