#ifndef S_INSTR_HDR
#define S_INSTR_HDR

#include <stddef.h>

namespace s {

typedef size_t Slot;
typedef size_t Block;
typedef size_t *BlockRef;

struct UserFunction;

struct Instr {
    Instr(int type);

    void dump(int level = 0);

    //top,ctx,no,ni,nf,ns,nc,close,get,set,call,ret,br,tbr
    enum {
        kGetRoot,
        kGetContext,
        kAllocObject,
        kAllocIntObject,
        kAllocFloatObject,
        kAllocStringObject,
        kAllocClosureObject,
        kCloseObject,
        kAccess,
        kAssign,
        kCall,
        kReturn,
        kBranch,
        kTestBranch
    };

    int m_type;
};

struct GetRootInstr : Instr {
    Slot m_slot;
};

struct GetContextInstr : Instr {
    Slot m_slot;
};

struct AllocObjectInstr : Instr {
    Slot m_targetSlot;
    Slot m_parentSlot;
};

struct AllocIntObjectInstr : Instr {
    Slot m_targetSlot;
    int m_value;
};

struct AllocFloatObjectInstr : Instr {
    Slot m_targetSlot;
    float m_value;
};

struct AllocStringObjectInstr : Instr {
    Slot m_targetSlot;
    const char *m_value;
};

struct AllocClosureObjectInstr : Instr {
    Slot m_targetSlot;
    Slot m_contextSlot;
    UserFunction *m_function;
};

struct CloseObjectInstr : Instr {
    Slot m_slot;
};

struct AccessInstr : Instr {
    Slot m_targetSlot;
    Slot m_objectSlot;
    Slot m_keySlot;
};

enum class AssignType {
    kPlain,
    kExisting,
    kShadowing
};

struct AssignInstr : Instr {
    Slot m_objectSlot;
    Slot m_valueSlot;
    Slot m_keySlot;
    AssignType m_assignType;
    static const char *asString(AssignInstr *instr);
};

struct CallInstr : Instr {
    Slot m_targetSlot;
    Slot m_functionSlot;
    Slot m_thisSlot;
    Slot *m_arguments;
    size_t m_length;
};

struct ReturnInstr : Instr {
    Slot m_returnSlot;
};

struct BranchInstr : Instr {
    Block m_block;
};

struct TestBranchInstr : Instr {
    Slot m_testSlot;
    Block m_trueBlock;
    Block m_falseBlock;
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
    bool m_isMethod;
    FunctionBody m_body;
};

}

#endif
