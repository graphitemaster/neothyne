#include "s_instr.h"

#include "u_log.h"
#include "u_vector.h"

namespace s {

Instr::Instr(int type)
    : m_type(type)
{
}

static inline void output(int level, const char *format, ...) {
    va_list va;
    va_start(va, format);
    u::Log::out("[script] => ");
    for (int i = 0; i < level; i++)
        u::Log::out("  ");
    u::Log::out("%s", u::format(format, va));
    va_end(va);
}

void UserFunction::dump(int level) {
    FunctionBody *body = &m_body;
    output(level, "fn %s (%zu), %zu slots [\n", m_name, m_arity, m_slots);
    for (size_t i = 0; i < body->m_length; i++) {
        output(level + 1, "cblock <%zu> [\n", i);
        InstrBlock *block = &body->m_blocks[i];
        for (size_t j = 0; j < block->m_length; j++) {
            Instr *instruction = block->m_instrs[j];
            instruction->dump(level + 2);
        }
        output(level + 1, "]\n");
    }
    output(level, "]\n");
}

void Instr::dump(int level) {
    // these get dumped later
    u::vector<UserFunction *> otherFunctions;
    switch (m_type) {
    case kGetRoot:
        output(level, "gr: %zu\n", ((GetRootInstr *)this)->m_slot);
        break;
    break; case kGetContext:
        output(level, "gc: %zu\n", ((GetContextInstr *)this)->m_slot);
        break;
    break; case kAllocObject:
        output(level, "ao: %zu = new object(%zu)\n",
            ((AllocObjectInstr *)this)->m_targetSlot,
            ((AllocObjectInstr *)this)->m_parentSlot);
        break;
    case kAllocIntObject:
        output(level, "aio: %zu = new int(%d)\n",
            ((AllocIntObjectInstr *)this)->m_targetSlot,
            ((AllocIntObjectInstr *)this)->m_value);
        break;
    case kAllocClosureObject:
        output(level, "aco: %zu = new fn(%zu)\n",
            ((AllocClosureObjectInstr *)this)->m_targetSlot,
            ((AllocClosureObjectInstr *)this)->m_contextSlot);
        otherFunctions.push_back(((AllocClosureObjectInstr *)this)->m_function);
        break;
    case kCloseObject:
        output(level, "co: %zu\n", ((CloseObjectInstr *)this)->m_slot);
        break;
    case kAccess:
        output(level, "ref: %zu = %zu . '%s'\n",
            ((AccessInstr *)this)->m_targetSlot,
            ((AccessInstr *)this)->m_objectSlot,
            ((AccessInstr *)this)->m_key);
        break;
    case kAssign:
        output(level, "mov: %zu . '%s' = %zu\n",
            ((AssignInstr *)this)->m_objectSlot,
            ((AssignInstr *)this)->m_key,
            ((AssignInstr *)this)->m_valueSlot);
        break;
    case kCall:
        // this one is a tad annoying but still doable
        output(level, "call: %zu = %zu ( ",
            ((CallInstr *)this)->m_targetSlot,
            ((CallInstr *)this)->m_functionSlot);
        for (size_t i = 0; i < ((CallInstr *)this)->m_length; ++i) {
            if (i) u::Log::out(", ");
            u::Log::out("%zu", ((CallInstr *)this)->m_arguments[i]);
        }
        u::Log::out(" )\n");
        break;
    case kReturn:
        output(level, "ret: %zu\n", ((ReturnInstr *)this)->m_returnSlot);
        break;
    case kBranch:
        output(level, "br: <%zu>\n", ((BranchInstr *)this)->m_block);
        break;
    case kTestBranch:
        output(level, "tbr: %zu ? <%zu> : <%zu>\n",
            ((TestBranchInstr *)this)->m_testSlot,
            ((TestBranchInstr *)this)->m_trueBlock,
            ((TestBranchInstr *)this)->m_falseBlock);
    }

    for (auto *function : otherFunctions) {
        function->dump(level);
        level++;
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

AllocClosureObjectInstr::AllocClosureObjectInstr(Slot target, Slot context, UserFunction *function)
    : Instr(kAllocClosureObject)
    , m_targetSlot(target)
    , m_contextSlot(context)
    , m_function(function)
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
