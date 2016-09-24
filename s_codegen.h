#ifndef S_CODEGEN_HDR
#define S_CODEGEN_HDR

#include "s_instr.h"

namespace s {

struct FunctionCodegen {
    FunctionCodegen();
    size_t newBlock();
    void addInstr(Instr *instr);
    Slot addAccess(Slot objectSlot, const char *identifer);
    void addAssign(Slot objectSlot, const char *name, Slot slot);
    void addCloseObject(Slot object);
    Slot addGetContext();
    Slot addAllocObject(Slot parent);
    Slot addAllocIntObject(int value);
    Slot addCall(Slot function, size_t *args, size_t length);
    void addTestBranch(Slot test, size_t **trueBranch, size_t **falseBranch);
    void addBranch(size_t **branch);
    void addReturn(Slot slot);
    UserFunction *build();
    void setName(const char *name);
private:
    const char *m_name;
    Slot m_slotBase;
    FunctionBody m_body;
};

}

#endif
