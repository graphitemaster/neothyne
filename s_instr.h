#ifndef S_INSTR_HDR
#define S_INSTR_HDR

#include <stddef.h>

namespace s {

using Slot = size_t;

enum InstructionType {
    kInvalid = -1,
    kGetRoot,
    kGetContext,
    kNewObject,
    kNewIntObject,
    kNewFloatObject,
    kNewArrayObject,
    kNewStringObject,
    kNewClosureObject,
    kCloseObject,
    kAccess,
    kAssign,
    kCall,
    kReturn,
    kSaveResult,
    kBranch,
    kTestBranch,
    kAccesStringKey,
    kAssignStringKey
};

enum AssignType {
    kAssignPlain,
    kAssignExisting,
    kAssignShadowing
};

struct Instruction {
    struct GetRoot;
    struct GetContext;
    struct NewObject;
    struct NewIntObject;
    struct NewFloatObject;
    struct NewArrayObject;
    struct NewStringObject;
    struct NewClosureObject;
    struct CloseObject;
    struct Access;
    struct Assign;
    struct Call;
    struct Return;
    struct SaveResult;
    struct Branch;
    struct TestBranch;
    struct AccessStringKey;
    struct AssignStringKey;

    static void dump(Instruction *instruction, int level);

    InstructionType m_type;
};

struct InstructionBlock {
    Instruction **m_instructions;
    size_t m_count;
};

struct FunctionBody {
    InstructionBlock *m_blocks;
    size_t m_count;
};

struct UserFunction {
    static void dump(UserFunction *function, int level);

    size_t m_arity; // first slots are reserved for parameters
    size_t m_slots;
    const char *m_name;
    bool m_isMethod;
    FunctionBody m_body;
};

struct Instruction::GetRoot : Instruction {
    Slot m_slot;
};

struct Instruction::GetContext : Instruction {
    Slot m_slot;
};

struct Instruction::NewObject : Instruction {
    Slot m_targetSlot;
    Slot m_parentSlot;
};

struct Instruction::NewIntObject : Instruction {
    Slot m_targetSlot;
    int m_value;
};

struct Instruction::NewFloatObject : Instruction {
    Slot m_targetSlot;
    float m_value;
};

struct Instruction::NewArrayObject : Instruction {
    Slot m_targetSlot;
};

struct Instruction::NewStringObject : Instruction {
    Slot m_targetSlot;
    char *m_value;
};

struct Instruction::NewClosureObject : Instruction {
    Slot m_targetSlot;
    Slot m_contextSlot;
    UserFunction *m_function;
};

struct Instruction::CloseObject : Instruction {
    Slot m_slot;
};

struct Instruction::Access : Instruction {
    Slot m_objectSlot;
    Slot m_keySlot;
    Slot m_targetSlot;
};

struct Instruction::Assign : Instruction {
    Slot m_objectSlot;
    Slot m_valueSlot;
    Slot m_keySlot;
    AssignType m_assignType;
};

struct Instruction::Call : Instruction {
    Slot m_functionSlot;
    Slot m_thisSlot;
    Slot *m_arguments;
    size_t m_count;
};

struct Instruction::Return : Instruction {
    Slot m_returnSlot;
};

struct Instruction::SaveResult : Instruction {
    Slot m_targetSlot;
};

struct Instruction::Branch : Instruction {
    size_t m_block;
};

struct Instruction::TestBranch : Instruction {
    Slot m_testSlot;
    size_t m_trueBlock;
    size_t m_falseBlock;
};

struct Instruction::AccessStringKey : Instruction {
    Slot m_objectSlot;
    const char *m_key;
    Slot m_targetSlot;
};

struct Instruction::AssignStringKey : Instruction {
    Slot m_objectSlot;
    Slot m_valueSlot;
    const char *m_key;
    AssignType m_assignType;
};

}

#endif
