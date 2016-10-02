#include "s_instr.h"

#include "u_log.h"
#include "u_vector.h"

namespace s {

static void indent(int level) {
    for (int i = 0; i < level; i++)
        u::Log::out("  ");
}

void Instruction::dump(Instruction *instruction, int level) {
    indent(level);
    switch (instruction->m_type) {
    case kGetRoot:
        u::Log::out("GetRoot:           %%%zu\n", ((GetRoot *)instruction)->m_slot);
        break;
    case kGetContext:
        u::Log::out("GetContext:        %%%zu\n", ((GetContext *)instruction)->m_slot);
        break;
    case kNewObject:
        u::Log::out("NewObject:         %%%zu => %%%zu\n",
            ((NewObject *)instruction)->m_targetSlot,
            ((NewObject *)instruction)->m_parentSlot);
        break;
    case kNewIntObject:
        u::Log::out("NewIntObject:      %%%zu = $%d\n",
            ((NewIntObject *)instruction)->m_targetSlot,
            ((NewIntObject *)instruction)->m_value);
        break;
    case kNewFloatObject:
        u::Log::out("NewFloatObject:    %%%zu = $%f\n",
            ((NewFloatObject *)instruction)->m_targetSlot,
            ((NewFloatObject *)instruction)->m_value);
        break;
    case kNewArrayObject:
        u::Log::out("NewArrayObject:    %%%zu\n",
            ((NewArrayObject *)instruction)->m_targetSlot);
        break;
    case kNewStringObject:
        u::Log::out("NewStringObject:   %%%zu = \"%s\"\n",
            ((NewStringObject *)instruction)->m_targetSlot,
            ((NewStringObject *)instruction)->m_value);
        break;
    case kNewClosureObject:
        u::Log::out("NewClosureObject:  %%%zu @ %%%zu\n",
            ((NewClosureObject *)instruction)->m_targetSlot,
            ((NewClosureObject *)instruction)->m_contextSlot);
        break;
    case kCloseObject:
        u::Log::out("CloseObject:       %%%zu\n",
            ((CloseObject *)instruction)->m_slot);
        break;
    case kAccess:
        u::Log::out("Access:            %%%zu = %%%zu . %%%zu\n",
            ((Access *)instruction)->m_targetSlot,
            ((Access *)instruction)->m_objectSlot,
            ((Access *)instruction)->m_keySlot);
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
        break;
    case kReturn:
        u::Log::out("Return:            %%%zu\n",
            ((Return *)instruction)->m_returnSlot);
        break;
    case kSaveResult:
        u::Log::out("SaveResult:        %%%zu\n",
            ((SaveResult *)instruction)->m_targetSlot);
        break;
    case kBranch:
        u::Log::out("Branch:            <%zu>\n",
            ((Branch *)instruction)->m_block);
        break;
    case kTestBranch:
        u::Log::out("TestBranch:        %%%zu ? <%zu> : <%zu>\n",
            ((TestBranch *)instruction)->m_testSlot,
            ((TestBranch *)instruction)->m_trueBlock,
            ((TestBranch *)instruction)->m_falseBlock);
        break;
    case kAccesStringKey:
        u::Log::out("Access:            %%%zu = %%%zu . \"%s\" [inlined]\n",
            ((AccessStringKey *)instruction)->m_targetSlot,
            ((AccessStringKey *)instruction)->m_objectSlot,
            ((AccessStringKey *)instruction)->m_key);
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
        break;
    default:
        break;
    }
}

void UserFunction::dump(UserFunction *function, int level) {
    u::vector<UserFunction *> otherFunctions;
    indent(level);
    FunctionBody *body = &function->m_body;
    u::Log::out("function %s (%zu), %zu slots {\n",
        function->m_name, function->m_arity, function->m_slots);
    level++;
    for (size_t i = 0; i < body->m_count; i++) {
        indent(level);
        u::Log::out("block <%zu> {\n", i);
        InstructionBlock *block = &body->m_blocks[i];
        level++;
        for (size_t k = 0; k < block->m_count; k++) {
            Instruction *instruction = block->m_instructions[k];
            Instruction::dump(instruction, level);
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
