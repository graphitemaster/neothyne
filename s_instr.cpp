#include "s_instr.h"

#include "u_log.h"
#include "u_vector.h"

namespace s {

static void indent(int level) {
    for (int i = 0; i < level; i++)
        u::Log::out("  ");
}

size_t Instruction::size(Instruction *instruction) {
    switch (instruction->m_type) {
    case kNewObject:              return sizeof(NewObject);
    case kNewIntObject:           return sizeof(NewIntObject);
    case kNewFloatObject:         return sizeof(NewFloatObject);
    case kNewArrayObject:         return sizeof(NewArrayObject);
    case kNewStringObject:        return sizeof(NewStringObject);
    case kNewClosureObject:       return sizeof(NewClosureObject);
    case kCloseObject:            return sizeof(CloseObject);
    case kSetConstraint:          return sizeof(SetConstraint);
    case kAccess:                 return sizeof(Access);
    case kFreeze:                 return sizeof(Freeze);
    case kAssign:                 return sizeof(Assign);
    case kCall:                   return sizeof(Call);
    case kReturn:                 return sizeof(Return);
    case kSaveResult:             return sizeof(SaveResult);
    case kBranch:                 return sizeof(Branch);
    case kTestBranch:             return sizeof(TestBranch);
    case kAccessStringKey:        return sizeof(AccessStringKey);
    case kAssignStringKey:        return sizeof(AssignStringKey);
    case kSetConstraintStringKey: return sizeof(SetConstraintStringKey);
    case kDefineFastSlot:         return sizeof(DefineFastSlot);
    case kReadFastSlot:           return sizeof(ReadFastSlot);
    case kWriteFastSlot:          return sizeof(WriteFastSlot);
    case kInvalid:                break;
    }
    u::Log::err("invalid instruction: %d\n", (int)instruction->m_type);
    U_ASSERT(0 && "internal error");
}

void Instruction::dump(Instruction **instructions, int level) {
    Instruction *instruction = *instructions;

    indent(level);
    switch (instruction->m_type) {
    case kNewObject:
        u::Log::out("NewObject:         %%%zu => %%%zu\n",
            ((NewObject *)instruction)->m_targetSlot,
            ((NewObject *)instruction)->m_parentSlot);
        *instructions = (Instruction *)((NewObject *)instruction + 1);
        break;
    case kNewIntObject:
        u::Log::out("NewIntObject:      %%%zu = $%d\n",
            ((NewIntObject *)instruction)->m_targetSlot,
            ((NewIntObject *)instruction)->m_value);
        *instructions = (Instruction *)((NewIntObject *)instruction + 1);
        break;
    case kNewFloatObject:
        u::Log::out("NewFloatObject:    %%%zu = $%f\n",
            ((NewFloatObject *)instruction)->m_targetSlot,
            ((NewFloatObject *)instruction)->m_value);
        *instructions = (Instruction *)((NewFloatObject *)instruction + 1);
        break;
    case kNewArrayObject:
        u::Log::out("NewArrayObject:    %%%zu\n",
            ((NewArrayObject *)instruction)->m_targetSlot);
        *instructions = (Instruction *)((NewArrayObject *)instruction + 1);
        break;
    case kNewStringObject:
        u::Log::out("NewStringObject:   %%%zu = \"%s\"\n",
            ((NewStringObject *)instruction)->m_targetSlot,
            ((NewStringObject *)instruction)->m_value);
        *instructions = (Instruction *)((NewStringObject *)instruction + 1);
        break;
    case kNewClosureObject:
        u::Log::out("NewClosureObject:  %%%zu\n",
            ((NewClosureObject *)instruction)->m_targetSlot);
        *instructions = (Instruction *)((NewClosureObject *)instruction + 1);
        break;
    case kCloseObject:
        u::Log::out("CloseObject:       %%%zu\n",
            ((CloseObject *)instruction)->m_slot);
        *instructions = (Instruction *)((CloseObject *)instruction + 1);
        break;
    case kAccess:
        u::Log::out("Access:            %%%zu = %%%zu . %%%zu\n",
            ((Access *)instruction)->m_targetSlot,
            ((Access *)instruction)->m_objectSlot,
            ((Access *)instruction)->m_keySlot);
        *instructions = (Instruction *)((Access *)instruction + 1);
        break;
    case kFreeze:
        u::Log::out("Freeze:            %%%zu\n",
            ((Freeze *)instruction)->m_slot);
        *instructions = (Instruction *)((Freeze *)instruction + 1);
        break;
    case kAssign:
        u::Log::out("Assign%s %%%zu . %%%zu = %%%zu\n",
            ((Assign *)instruction)->m_assignType == kAssignPlain
                ? "(plain):    "
                : ((Assign *)instruction)->m_assignType == kAssignExisting
                    ? "(existing): "
                    : "(shadowing):",
            ((Assign *)instruction)->m_objectSlot,
            ((Assign *)instruction)->m_keySlot,
            ((Assign *)instruction)->m_valueSlot);
        *instructions = (Instruction *)((Assign *)instruction + 1);
        break;
    case kSetConstraint:
        u::Log::out("SetConstraint:     %%%zu . %%%zu : %%%zu\n",
            ((SetConstraint *)instruction)->m_objectSlot,
            ((SetConstraint *)instruction)->m_keySlot,
            ((SetConstraint *)instruction)->m_constraintSlot);
        break;
    case kCall:
        u::Log::out("Call:              %%%zu . %%%zu ( ",
            ((Call *)instruction)->m_thisSlot,
            ((Call *)instruction)->m_functionSlot);
        for (size_t i = 0; i < ((Call *)instruction)->m_count; i++) {
            if (i) u::Log::out(", ");
            u::Log::out("%%%zu", ((Call *)instruction)->m_arguments[i]);
        }
        u::Log::out(" )\n");
        *instructions = (Instruction *)((Call *)instruction + 1);
        break;
    case kReturn:
        u::Log::out("Return:            %%%zu\n",
            ((Return *)instruction)->m_returnSlot);
        *instructions = (Instruction *)((Return *)instruction + 1);
        break;
    case kSaveResult:
        u::Log::out("SaveResult:        %%%zu\n",
            ((SaveResult *)instruction)->m_targetSlot);
        *instructions = (Instruction *)((SaveResult *)instruction + 1);
        break;
    case kBranch:
        u::Log::out("Branch:            <%zu>\n",
            ((Branch *)instruction)->m_block);
        *instructions = (Instruction *)((Branch *)instruction + 1);
        break;
    case kTestBranch:
        u::Log::out("TestBranch:        %%%zu ? <%zu> : <%zu>\n",
            ((TestBranch *)instruction)->m_testSlot,
            ((TestBranch *)instruction)->m_trueBlock,
            ((TestBranch *)instruction)->m_falseBlock);
        *instructions = (Instruction *)((TestBranch *)instruction + 1);
        break;
    case kAccessStringKey:
        u::Log::out("Access:            %%%zu = %%%zu . \"%s\" [inlined]\n",
            ((AccessStringKey *)instruction)->m_targetSlot,
            ((AccessStringKey *)instruction)->m_objectSlot,
            ((AccessStringKey *)instruction)->m_key);
        *instructions = (Instruction *)((AccessStringKey *)instruction + 1);
        break;
    case kAssignStringKey:
        u::Log::out("Assign%s %%%zu . \"%s\" = %%%zu [inlined]\n",
            ((AssignStringKey *)instruction)->m_assignType == kAssignPlain
                ? "(plain):    "
                : ((AssignStringKey *)instruction)->m_assignType == kAssignExisting
                    ? "(existing): "
                    : "(shadowing):",
            ((AssignStringKey *)instruction)->m_objectSlot,
            ((AssignStringKey *)instruction)->m_key,
            ((AssignStringKey *)instruction)->m_valueSlot);
        *instructions = (Instruction *)((AssignStringKey *)instruction + 1);
        break;
    case kSetConstraintStringKey:
        u::Log::out("SetConstraint:     %%%zu . '%.*s' : %%%zu [optimized]\n",
            ((SetConstraintStringKey *)instruction)->m_objectSlot,
            (int)((SetConstraintStringKey *)instruction)->m_keyLength,
            ((SetConstraintStringKey *)instruction)->m_key,
            ((SetConstraintStringKey *)instruction)->m_constraintSlot);
        *instructions = (Instruction *)((SetConstraintStringKey *)instruction + 1);
        break;
    case kDefineFastSlot:
        u::Log::out("DefineFastSlot     &%%%zu = %%%zu . '%.*s' [optimized]\n",
            ((DefineFastSlot *)instruction)->m_targetSlot,
            ((DefineFastSlot *)instruction)->m_objectSlot,
            (int)((DefineFastSlot *)instruction)->m_keyLength,
            ((DefineFastSlot *)instruction)->m_key);
        *instructions = (Instruction *)((DefineFastSlot *)instruction + 1);
        break;
    case kReadFastSlot:
        u::Log::out("ReadFastSlot       %%%zu = &%%%zu [optimized]\n",
            ((ReadFastSlot *)instruction)->m_targetSlot,
            ((ReadFastSlot *)instruction)->m_sourceSlot);
        *instructions = (Instruction *)((ReadFastSlot *)instruction + 1);
        break;
    case kWriteFastSlot:
        u::Log::out("WriteFastSlot      &%%%zu = %%%zu [optimized]\n",
            ((WriteFastSlot *)instruction)->m_targetSlot,
            ((WriteFastSlot *)instruction)->m_sourceSlot);
        *instructions = (Instruction *)((WriteFastSlot *)instruction + 1);
        break;
    default:
        break;
    }
}

void UserFunction::dump(UserFunction *function, int level) {
    u::vector<UserFunction *> otherFunctions;
    indent(level);
    FunctionBody *body = &function->m_body;
    u::Log::out("function %s (%zu), %zu slots, %zu fast slots {\n",
        function->m_name, function->m_arity, function->m_slots, function->m_fastSlots);
    level++;
    for (size_t i = 0; i < body->m_count; i++) {
        indent(level);
        u::Log::out("block <%zu> {\n", i);
        InstructionBlock *block = &body->m_blocks[i];
        level++;
        Instruction *instruction = block->m_instructions;
        while (instruction != block->m_instructionsEnd) {
            Instruction::dump(&instruction, level);
            if (instruction->m_type == kNewClosureObject)
                otherFunctions.push_back(((Instruction::NewClosureObject *)instruction)->m_function);
        }
        level--;
        indent(level);
        u::Log::out("}\n");
    }
    level--;
    indent(level);
    u::Log::out("}\n");
    for (UserFunction *function : otherFunctions)
        UserFunction::dump(function, level);
}

}
