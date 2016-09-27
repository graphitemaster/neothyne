#include <string.h>

#include "s_object.h"
#include "s_generator.h"

#include "u_assert.h"
#include "u_vector.h"
#include "u_log.h"

namespace s {

Block Generator::newBlock() {
    U_ASSERT(m_terminated);
    FunctionBody *body = &m_body;
    body->m_length++;
    body->m_blocks = (InstrBlock *)neoRealloc(body->m_blocks, body->m_length * sizeof(InstrBlock));
    body->m_blocks[body->m_length - 1].m_instrs = nullptr;
    body->m_blocks[body->m_length - 1].m_length = 0;
    m_terminated = false;
    return body->m_length - 1;
}

void Generator::terminate() {
    // return null
    addReturn(m_slotBase++);
}

void Generator::addInstr(Instr *instruction) {
    U_ASSERT(!m_terminated);
    FunctionBody *body = &m_body;
    InstrBlock *block = &body->m_blocks[body->m_length - 1];
    block->m_length++;
    block->m_instrs = (Instr **)neoRealloc(block->m_instrs, block->m_length * sizeof(Instr *));
    block->m_instrs[block->m_length - 1] = instruction;
    switch (instruction->m_type) {
    case Instr::kBranch:
    case Instr::kTestBranch:
    case Instr::kReturn:
        m_terminated = true;
        break;
    default:
        break;
    }
}

#define newInstruction(TYPE) \
    allocInstruction<TYPE##Instr>(Instr::k##TYPE)

template <typename T>
inline T *allocInstruction(int type) {
    T *instruction = (T *)neoCalloc(sizeof *instruction, 1);
    instruction->m_type = type;
    return instruction;
}

Slot Generator::addAccess(Slot objectSlot, Slot keySlot) {
    auto *instruction = newInstruction(Access);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_objectSlot = objectSlot;
    instruction->m_keySlot = keySlot;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

void Generator::addAssign(Slot object, Slot keySlot, Slot slot, AssignType type) {
    auto *instruction = newInstruction(Assign);
    instruction->m_objectSlot = object;
    instruction->m_valueSlot = slot;
    instruction->m_keySlot = keySlot;
    instruction->m_assignType = type;
    addInstr((Instr *)instruction);
}

void Generator::addCloseObject(Slot object) {
    auto *instruction = newInstruction(CloseObject);
    instruction->m_slot = object;
    addInstr((Instr *)instruction);
}

Slot Generator::addGetContext() {
    auto *instruction = newInstruction(GetContext);
    instruction->m_slot = m_slotBase++;
    addInstr((Instr *)instruction);
    return instruction->m_slot;
}

Slot Generator::addAllocObject(Slot parent) {
    auto *instruction = newInstruction(AllocObject);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_parentSlot = parent;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addAllocClosureObject(Slot contextSlot, UserFunction *function) {
    auto *instruction = newInstruction(AllocClosureObject);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_contextSlot = contextSlot;
    instruction->m_function = function;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addAllocIntObject(Slot contextSlot, int value) {
    (void)contextSlot;
    auto *instruction = newInstruction(AllocIntObject);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addAllocFloatObject(Slot contextSlot, float value) {
    (void)contextSlot;
    auto *instruction = newInstruction(AllocFloatObject);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addAllocArrayObject(Slot contextSlot) {
    (void)contextSlot;
    auto *instruction = newInstruction(AllocArrayObject);
    instruction->m_targetSlot = m_slotBase++;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addAllocStringObject(Slot contextSlot, const char *value) {
    (void)contextSlot;
    auto *instruction = newInstruction(AllocStringObject);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_value = value;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addCall(Slot function, Slot thisSlot, Slot *arguments, size_t length) {
    auto *instruction = newInstruction(Call);
    instruction->m_targetSlot = m_slotBase++;
    instruction->m_functionSlot = function;
    instruction->m_thisSlot = thisSlot;
    instruction->m_length = length;
    instruction->m_arguments = arguments;
    addInstr((Instr *)instruction);
    return instruction->m_targetSlot;
}

Slot Generator::addCall(Slot function, Slot thisSlot) {
    return addCall(function, thisSlot, nullptr, 0);
}

Slot Generator::addCall(Slot function, Slot thisSlot, Slot arg0) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 1);
    arguments[0] = arg0;
    return addCall(function, thisSlot, arguments, 1);
}

Slot Generator::addCall(Slot function, Slot thisSlot, Slot arg0, Slot arg1) {
    Slot *arguments = (Slot *)neoMalloc(sizeof *arguments * 2);
    arguments[0] = arg0;
    arguments[1] = arg1;
    return addCall(function, thisSlot, arguments, 2);
}

void Generator::addTestBranch(Slot test, Block **trueBranch, Block **falseBranch) {
    auto *instruction = newInstruction(TestBranch);
    instruction->m_testSlot = test;
    *trueBranch = &instruction->m_trueBlock;
    *falseBranch = &instruction->m_falseBlock;
    addInstr((Instr *)instruction);
}

void Generator::addBranch(Block **branch) {
    auto *instruction = newInstruction(Branch);
    *branch = &instruction->m_block;
    addInstr((Instr *)instruction);
}

void Generator::addReturn(Slot slot) {
    auto *instruction = newInstruction(Return);
    instruction->m_returnSlot = slot;
    addInstr((Instr *)instruction);
}

UserFunction *Generator::build() {
    U_ASSERT(m_terminated);
    UserFunction *function = (UserFunction *)neoCalloc(sizeof *function, 1);
    function->m_arity = m_length;
    function->m_slots = m_slotBase;
    function->m_name = m_name;
    function->m_body = m_body;
    function->m_isMethod = false;
    return function;
}

// Searches through the function to find slots whose only value is used as a
// parameter to other instructions which does not escape
static size_t findPrimitiveSlots(UserFunction *function, bool **slots) {
    size_t primitiveSlots = 0;

    *slots = (bool *)neoMalloc(sizeof **slots * function->m_slots);
    // default assumption is that the slot is primitive
    for (size_t i = 0; i < function->m_slots; i++)
        (*slots)[i] = true;

    #define MARK(SLOT) do { (*slots)[(SLOT)] = false; primitiveSlots++; } while (0)

    // for all blocks in the function
    for (size_t i = 0; i < function->m_body.m_length; i++) {
        InstrBlock *block = &function->m_body.m_blocks[i];
        // for all instructions in the block
        for (size_t j = 0; j < block->m_length; j++) {
            Instr *instruction = block->m_instrs[j];
            switch (instruction->m_type) {
            case Instr::kAllocObject:
                MARK(((AllocObjectInstr*)instruction)->m_parentSlot);
                break;
            case Instr::kAllocClosureObject:
                MARK(((AllocClosureObjectInstr*)instruction)->m_contextSlot);
                break;
            case Instr::kAccess:
                MARK(((AccessInstr*)instruction)->m_objectSlot);
                break;
            case Instr::kAssign:
                MARK(((AssignInstr*)instruction)->m_objectSlot);
                MARK(((AssignInstr*)instruction)->m_valueSlot);
                break;
            case Instr::kCall:
                MARK(((CallInstr*)instruction)->m_functionSlot);
                MARK(((CallInstr*)instruction)->m_thisSlot);
                for (size_t k = 0; k < ((CallInstr*)instruction)->m_length; k++)
                    MARK(((CallInstr*)instruction)->m_arguments[k]);
                break;
            case Instr::kReturn:
                MARK(((ReturnInstr*)instruction)->m_returnSlot);
                break;
            case Instr::kTestBranch:
                MARK(((TestBranchInstr*)instruction)->m_testSlot);
                break;
            default: break;
            }
        }
    }

    return primitiveSlots;
}

UserFunction *Generator::optimize(UserFunction *function) {
    // find all primitive slots first
    bool *primitiveSlots = nullptr;
    size_t primitiveCount = findPrimitiveSlots(function, &primitiveSlots);
    size_t inlinedCount = 0;

    if (primitiveCount == 0) {
        neoFree(primitiveSlots);
        return function;
    }

    Generator *generator = (Generator *)neoCalloc(sizeof *generator, 1);
    generator->m_slotBase = 0;
    generator->m_terminated = true;

    u::vector<const char *> slotTable;
    for (size_t i = 0; i < function->m_body.m_length; i++) {
        InstrBlock *block = &function->m_body.m_blocks[i];
        generator->newBlock();
        for (size_t j = 0; j < block->m_length; j++) {
            Instr *instruction = block->m_instrs[j];
            auto *alloc = (AllocStringObjectInstr *)instruction;
            auto *access = (AccessInstr *)instruction;
            auto *assign = (AssignInstr *)instruction;
            if (instruction->m_type == Instr::kAllocStringObject
                && primitiveSlots[alloc->m_targetSlot])
            {
                if (slotTable.size() < alloc->m_targetSlot + 1)
                    slotTable.resize(alloc->m_targetSlot + 1);
                slotTable[alloc->m_targetSlot] = alloc->m_value;
                continue;
            }
            if (instruction->m_type == Instr::kAccess
                && access->m_keySlot < slotTable.size() && slotTable[access->m_keySlot])
            {
                auto *insert = (AccessKeyInstr *)neoMalloc(sizeof(AccessKeyInstr));
                insert->m_type = Instr::kAccessKey;
                insert->m_targetSlot = access->m_targetSlot;
                insert->m_objectSlot = access->m_objectSlot;
                insert->m_key = slotTable[access->m_keySlot];
                generator->addInstr((Instr *)insert);
                inlinedCount++;
                continue;
            }
            if (instruction->m_type == Instr::kAssign
                && assign->m_keySlot < slotTable.size() && slotTable[assign->m_keySlot])
            {
                auto *insert = (AssignKeyInstr *)neoMalloc(sizeof(AssignKeyInstr));
                insert->m_type = Instr::kAssignKey;
                insert->m_objectSlot = assign->m_objectSlot;
                insert->m_valueSlot = assign->m_valueSlot;
                insert->m_key = slotTable[assign->m_keySlot];
                insert->m_assignType = assign->m_assignType;
                generator->addInstr((Instr *)insert);
                inlinedCount++;
                continue;
            }
            generator->addInstr(instruction);
        }
    }

    UserFunction *newFunction = generator->build();
    newFunction->m_slots = function->m_slots;
    newFunction->m_arity = function->m_arity;
    newFunction->m_name = function->m_name;
    newFunction->m_isMethod = function->m_isMethod;

    u::Log::out("[script] => inlined %zu of %zu primitive accesses/asignments\n",
        inlinedCount, primitiveCount);

    neoFree(primitiveSlots);
    return newFunction;
}

}
