#ifndef S_INSTR_HDR
#define S_INSTR_HDR

#include <stddef.h>

namespace s {

typedef size_t Slot;

struct Instr {
    Instr(int type);

    enum {
        kGetRoot,        // gr
        kGetContext,     // gc
        kAllocObject,    // ao
        kAllocIntObject, // aio
        kCloseObject,    // co
        kAccess,         // ref
        kAssign,         // mov
        kCall,           // call
        kReturn,         // ret
        kBranch,         // br
        kTestBranch      // tbr
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

struct CloseObjectInstr : Instr {
    CloseObjectInstr(Slot slot);
    Slot m_slot;
};

struct AccessInstr : Instr {
    AccessInstr(Slot target, Slot object, const char *key);
    Slot m_targetSlot;
    Slot m_objectSlot;
    const char *m_key;
};

struct AssignInstr : Instr {
    AssignInstr(Slot object, Slot value, const char *key);
    Slot m_objectSlot;
    Slot m_valueSlot;
    const char *m_key;
};

struct CallInstr : Instr {
    CallInstr(Slot target, Slot function, size_t *args, size_t length);
    Slot m_targetSlot;
    Slot m_functionSlot;
    size_t *m_argsData;
    size_t m_argsLength;
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

}

#endif
