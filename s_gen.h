#ifndef S_GEN_HDR
#define S_GEN_HDR

#include <stdint.h>

#include "s_object.h"

namespace s {

struct BlockRef {
    size_t m_block;
    uintptr_t m_distance;
};

struct Gen {
    static BlockRef newBlockRef(Gen *gen, unsigned char *instruction, unsigned char *address);
    static void setBlockRef(Gen *gen, BlockRef ref, size_t value);

    static void useRangeStart(Gen *gen, FileRange *range);
    static void useRangeEnd(Gen *gen, FileRange *range);
    static FileRange *newRange(char *text);
    static void delRange(FileRange *range);

    static size_t newBlock(Gen *gen);

    static void terminate(Gen *gen);

    static Slot addAccess(Gen *gen, Slot objectSlot, Slot keySlot);
    static void addAssign(Gen *gen, Slot objectSlot, Slot keySlot, Slot slot, AssignType assignType);

    static void addCloseObject(Gen *gen, Slot objectSlot);

    static Slot addGetContext(Gen *gen);

    static Slot addNewObject(Gen *gen, Slot parentSlot);
    static Slot addNewIntObject(Gen *gen, Slot contextSlot, int value);
    static Slot addNewFloatObject(Gen *gen, Slot contextSlot, float value);
    static Slot addNewArrayObject(Gen *gen, Slot contextSlot);
    static Slot addNewStringObject(Gen *gen, Slot contextSlot, const char *value);
    static Slot addNewClosureObject(Gen *gen, Slot contextSlot, UserFunction *function);

    static Slot addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot *arguments, size_t count);
    static Slot addCall(Gen *gen, Slot functionSlot, Slot thisSlot);
    static Slot addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0);
    static Slot addCall(Gen *gen, Slot functionSlot, Slot thisSlot, Slot argument0, Slot argument1);

    static void addTestBranch(Gen *gen, Slot testSlot, BlockRef *trueBranch, BlockRef *falseBranch);
    static void addBranch(Gen *gen, BlockRef *branch);

    static void addReturn(Gen *gen, Slot slot);

    static void addFreeze(Gen *gen, Slot object);

    static Slot addDefineFastSlot(Gen *gen, Slot objectSlot, const char *key);
    static void addReadFastSlot(Gen *gen, Slot sourceSlot, Slot targetSlot);
    static void addWriteFastSlot(Gen *gen, Slot sourceSlot, Slot targetSlot);

    static UserFunction *buildFunction(Gen *gen);

    static UserFunction *optimize(UserFunction *function);

    static UserFunction *inlinePass(UserFunction *function, bool *primitiveSlots);
    static UserFunction *predictPass(UserFunction *function);
    static UserFunction *fastSlotPass(UserFunction *function);

    static size_t scopeEnter(Gen *gen);
    static void scopeLeave(Gen *gen, size_t backup);

    static void copyFunctionStats(UserFunction *from, UserFunction *to);

private:
    friend struct Parser;

    static void addInstruction(Gen *gen, size_t size, Instruction *instruction);

    const char *m_name;
    size_t m_count;
    Slot m_scope;
    Slot m_slot;
    Slot m_fastSlot;
    bool m_blockTerminated;
    FileRange *m_currentRange;
    FunctionBody m_body;
    bool m_hasVariadicTail;
};

}

#endif
