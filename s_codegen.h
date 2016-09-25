#ifndef S_CODEGEN_HDR
#define S_CODEGEN_HDR

#include "s_instr.h"

namespace s {

struct FunctionCodegen {
    FunctionCodegen(const char **arguments, size_t length, const char *name);

    size_t newBlock();
    void addInstr(Instr *instr);
    Slot addAccess(Slot objectSlot, const char *identifer);
    void addAssign(Slot objectSlot, const char *name, Slot slot);
    void addCloseObject(Slot object);
    Slot addGetContext();
    Slot addAllocObject(Slot parent);
    Slot addAllocIntObject(int value);
    Slot addCall(Slot function, Slot *arguments, size_t length);
    Slot addCall(Slot function, Slot arg0, Slot arg1); // specialization just for binary operator calls
    void addTestBranch(Slot test, size_t **trueBranch, size_t **falseBranch);
    void addBranch(size_t **branch);
    void addReturn(Slot slot);
    UserFunction *build();

    // Note: these are dangerous to use
    Slot getScope() const { return m_scope; }
    void setScope(Slot scope) { m_scope = scope; }
private:
    const char **m_arguments;
    size_t m_length;
    const char *m_name;
    Slot m_scope;
    Slot m_slotBase;
    FunctionBody m_body;
};

}

#endif
