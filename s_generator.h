#ifndef S_CODEGEN_HDR
#define S_CODEGEN_HDR

#include "s_instr.h"

namespace s {

struct Generator {
    Block newBlock();

    // terminates with "return null"
    void terminate();

    void addInstr(Instr *instr);

    Slot addAccess(Slot objectSlot, Slot keySlot);
    void addAssign(Slot objectSlot, Slot keySlot, Slot slot, AssignType type);

    void addCloseObject(Slot object);

    Slot addGetContext();

    Slot addAllocObject(Slot parent);
    Slot addAllocIntObject(Slot contextSlot, int value);
    Slot addAllocFloatObject(Slot contextSlot, float value);
    Slot addAllocArrayObject(Slot contextSlot);
    Slot addAllocStringObject(Slot contextSlot, const char *value);

    Slot addCall(Slot function, Slot thisSlot, Slot *arguments, size_t length);
    Slot addCall(Slot function, Slot thisSlot, Slot arg0, Slot arg1); // specialization just for relational operators
    Slot addCall(Slot function, Slot thisSlot, Slot arh0);            // specialization for binary operators (lhs.operator+(arg0))
    Slot addCall(Slot function, Slot thisSlot);

    void addTestBranch(Slot test, Block **trueBranch, Block **falseBranch);
    void addBranch(Block **branch);
    void addReturn(Slot slot);

    Slot addAllocClosureObject(Slot contextSlot, UserFunction *function);

    UserFunction *build();

private:
    friend struct Parser;

    const char **m_arguments;
    size_t m_length;
    const char *m_name;
    Slot m_scope;
    Slot m_slotBase;
    FunctionBody m_body;
    bool m_terminated;
};

}

#endif
