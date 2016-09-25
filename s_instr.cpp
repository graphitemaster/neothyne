#include "s_instr.h"

#include "u_log.h"

namespace s {

Instr::Instr(int type)
    : m_type(type)
{
}

void Instr::dump() {
    switch (m_type) {
    case kGetRoot:
        u::Log::out("[script] =>     gr: %zu\n", ((GetRootInstr *)this)->m_slot);
        break;
    break; case kGetContext:
        u::Log::out("[script] =>     gc: %zu\n", ((GetContextInstr *)this)->m_slot);
        break;
    break; case kAllocObject:
        u::Log::out("[script] =>     ao: %zu = new object(%zu)\n",
            ((AllocObjectInstr *)this)->m_targetSlot,
            ((AllocObjectInstr *)this)->m_parentSlot);
        break;
    case kAllocIntObject:
        u::Log::out("[script] =>     aio: %zu = new int(%d)\n",
            ((AllocIntObjectInstr *)this)->m_targetSlot,
            ((AllocIntObjectInstr *)this)->m_value);
        break;
    case kCloseObject:
        u::Log::out("[script] =>     co: %zu\n", ((CloseObjectInstr *)this)->m_slot);
        break;
    case kAccess:
        u::Log::out("[script] =>     ref: %zu = %zu . '%s'\n",
            ((AccessInstr *)this)->m_targetSlot,
            ((AccessInstr *)this)->m_objectSlot,
            ((AccessInstr *)this)->m_key);
        break;
    case kAssign:
        u::Log::out("[script] =>     mov: %zu . '%s' = %zu\n",
            ((AssignInstr *)this)->m_objectSlot,
            ((AssignInstr *)this)->m_key,
            ((AssignInstr *)this)->m_valueSlot);
        break;
    case kCall:
        u::Log::out("[script] =>     call: %zu = %zu ( ",
            ((CallInstr *)this)->m_targetSlot,
            ((CallInstr *)this)->m_functionSlot);
        for (size_t i = 0; i < ((CallInstr *)this)->m_length; ++i) {
            if (i) u::Log::out(", ");
            u::Log::out("%zu", ((CallInstr *)this)->m_arguments[i]);
        }
        u::Log::out(" )\n");
        break;
    case kReturn:
        u::Log::out("[script] =>     ret: %zu\n", ((ReturnInstr *)this)->m_returnSlot);
        break;
    case kBranch:
        u::Log::out("[script] =>     br: <%zu>\n", ((BranchInstr *)this)->m_block);
        break;
    case kTestBranch:
        u::Log::out("[script] =>     tbr: %zu ? <%zu> : <%zu>\n",
            ((TestBranchInstr *)this)->m_testSlot,
            ((TestBranchInstr *)this)->m_trueBlock,
            ((TestBranchInstr *)this)->m_falseBlock);
    }
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
