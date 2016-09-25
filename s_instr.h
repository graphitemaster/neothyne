#ifndef S_INSTR_HDR
#define S_INSTR_HDR

#include <stddef.h>

namespace s {

typedef size_t Slot;

struct UserFunction;

struct Instr {
    Instr(int type);

    void dump(int level = 0);

    enum {
        kGetRoot,            // gr
        kGetContext,         // gc
        kAllocObject,        // ao
        kAllocIntObject,     // aio
        kAllocFloatObject,   // afo
        kAllocStringObject,  // aso
        kAllocClosureObject, // aco
        kCloseObject,        // co
        kAccess,             // ref
        kAssign,             // mov
        kAssignExisting,     // move
        kCall,               // call
        kReturn,             // ret
        kBranch,             // br
        kTestBranch          // tbr
    };

    int m_type;
};

struct GetRootInstr : Instr {
    GetRootInstr(Slot slot);
    Slot m_slot;
};

struct GetContextInstr : Instr {
    GetContextInstr(Slot slot);
    Slot m_slot;
};

struct AllocObjectInstr : Instr {
    AllocObjectInstr(Slot target, Slot parent);
    Slot m_targetSlot;
    Slot m_parentSlot;
};

struct AllocIntObjectInstr : Instr {
    AllocIntObjectInstr(Slot target, int value);
    Slot m_targetSlot;
    int m_value;
};

struct AllocFloatObjectInstr : Instr {
    AllocFloatObjectInstr(Slot target, float value);
    Slot m_targetSlot;
    float m_value;
};

struct AllocStringObjectInstr : Instr {
    AllocStringObjectInstr(Slot target, const char *value);
    Slot m_targetSlot;
    const char *m_value;
};

struct AllocClosureObjectInstr : Instr {
    AllocClosureObjectInstr(Slot target, Slot context, UserFunction *function);
    Slot m_targetSlot;
    Slot m_contextSlot;
    UserFunction *m_function;
};

struct CloseObjectInstr : Instr {
    CloseObjectInstr(Slot slot);
    Slot m_slot;
};

struct AccessInstr : Instr {
    AccessInstr(Slot target, Slot object, Slot keySlot);
    Slot m_targetSlot;
    Slot m_objectSlot;
    Slot m_keySlot;
};

struct AssignInstr : Instr {
    AssignInstr(Slot object, Slot value, Slot keySlot);
    Slot m_objectSlot;
    Slot m_valueSlot;
    Slot m_keySlot;
};

struct AssignExistingInstr : Instr {
    AssignExistingInstr(Slot object, Slot value, Slot keySlot);
    Slot m_objectSlot;
    Slot m_valueSlot;
    Slot m_keySlot;
};

struct CallInstr : Instr {
    CallInstr(Slot target, Slot function, Slot *arguments, size_t length);
    Slot m_targetSlot;
    Slot m_functionSlot;
    Slot *m_arguments;
    size_t m_length;
};

struct ReturnInstr : Instr {
    ReturnInstr(Slot slot);
    Slot m_returnSlot;
};

struct BranchInstr : Instr {
    BranchInstr(size_t block);
    size_t m_block;
};

struct TestBranchInstr : Instr {
    TestBranchInstr(Slot testSlot, size_t trueBlock, size_t falseBlock);
    Slot m_testSlot;
    size_t m_trueBlock;
    size_t m_falseBlock;
};

struct InstrBlock {
    Instr **m_instrs;
    size_t m_length;
};

struct FunctionBody {
    InstrBlock *m_blocks;
    size_t m_length;
};

struct UserFunction {
    void dump(int level = 0);
    size_t m_arity;
    size_t m_slots;
    const char *m_name;
    FunctionBody m_body;
};

}

#endif
