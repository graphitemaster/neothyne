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
        output(level + 1, "block <%zu> [\n", i);
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
    case kAllocFloatObject:
        output(level, "afo: %zu = new float(%f)\n",
            ((AllocFloatObjectInstr *)this)->m_targetSlot,
            ((AllocFloatObjectInstr *)this)->m_value);
        break;
    case kAllocStringObject:
        output(level, "aso: %zu = new string(\"%s\")\n",
            ((AllocStringObjectInstr *)this)->m_targetSlot,
            ((AllocStringObjectInstr *)this)->m_value);
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
        output(level, "ref: %zu = %zu . %zu\n",
            ((AccessInstr *)this)->m_targetSlot,
            ((AccessInstr *)this)->m_objectSlot,
            ((AccessInstr *)this)->m_keySlot);
        break;
    case kAssignNormal:
        output(level, "mov: %zu . %zu = %zu\n",
            ((AssignNormalInstr *)this)->m_objectSlot,
            ((AssignNormalInstr *)this)->m_keySlot,
            ((AssignNormalInstr *)this)->m_valueSlot);
        break;
    case kAssignExisting:
        output(level, "move: %zu . %zu = %zu\n",
            ((AssignExistingInstr *)this)->m_objectSlot,
            ((AssignExistingInstr *)this)->m_keySlot,
            ((AssignExistingInstr *)this)->m_valueSlot);
        break;
    case kCall:
        // this one is a tad annoying but still doable
        output(level, "call: %zu = %zu . %zu ( ",
            ((CallInstr *)this)->m_targetSlot,
            ((CallInstr *)this)->m_functionSlot,
            ((CallInstr *)this)->m_thisSlot);
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

    for (auto *function : otherFunctions)
        function->dump(level);
}

}
