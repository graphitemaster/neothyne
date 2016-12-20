#ifndef S_INSTR_HDR
#define S_INSTR_HDR

#include <stddef.h>

namespace s {

using Slot = size_t;

struct FileRange;
struct Object;

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
    kFreeze,
    kAssign,
    kCall,
    kReturn,
    kSaveResult,
    kBranch,
    kTestBranch,
    kAccessStringKey,
    kAssignStringKey,
    kDefineFastSlot,
    kReadFastSlot,
    kWriteFastSlot
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
    struct Freeze;
    struct Assign;
    struct Call;
    struct Return;
    struct SaveResult;
    struct Branch;
    struct TestBranch;
    struct AccessStringKey;
    struct AssignStringKey;
    struct DefineFastSlot;
    struct ReadFastSlot;
    struct WriteFastSlot;

    static void dump(Instruction **instructions, int level);
    static size_t size(Instruction *instruction);

    InstructionType m_type;
    FileRange *m_belongsTo;
};

struct InstructionBlock {
    Instruction *m_instructions;
    Instruction *m_instructionsEnd;
};

struct FunctionBody {
    InstructionBlock *m_blocks;
    size_t m_count;
};

struct UserFunction {
    static void dump(UserFunction *function, int level);

    size_t m_arity;     // first slots are reserved for parameters
    size_t m_slots;     // generic slots
    size_t m_fastSlots; // fast slots (register renames essentially)
    const char *m_name;
    bool m_isMethod;
    bool m_hasVariadicTail;
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
    Object *m_intObject;
};

struct Instruction::NewFloatObject : Instruction {
    Slot m_targetSlot;
    float m_value;
    Object *m_floatObject;
};

struct Instruction::NewArrayObject : Instruction {
    Slot m_targetSlot;
};

struct Instruction::NewStringObject : Instruction {
    Slot m_targetSlot;
    const char *m_value;
    Object *m_stringObject;
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

struct Instruction::Freeze : Instruction {
    Slot m_slot;
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

struct Instruction::DefineFastSlot : Instruction {
    Slot m_targetSlot;
    Slot m_objectSlot;
    const char *m_key;
    size_t m_keyLength;
    size_t m_keyHash;
};

struct Instruction::ReadFastSlot : Instruction {
    Slot m_sourceSlot;
    Slot m_targetSlot;
};

struct Instruction::WriteFastSlot : Instruction {
    Slot m_sourceSlot;
    Slot m_targetSlot;
};

}

#endif
