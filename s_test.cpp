#include "s_object.h"
#include "s_instr.h"

#include "u_log.h"

namespace s {

template <typename T>
Instr *allocInstr(const T &value) {
    T *instr = allocate<T>();
    *instr = value;
    return (Instr *)instr;
}

void test() {
    Object *root = createRoot();

    // ackermans function
    UserFunction *ackFn = allocate<UserFunction>();
    ackFn->m_arity = 2;
    ackFn->m_slots = 24;
    ackFn->m_name = "ack";
    ackFn->m_body.m_length = 5;
    ackFn->m_body.m_blocks = allocate<InstrBlock>(5);

    InstrBlock *blocks = ackFn->m_body.m_blocks;
    // allocate instructions for our blocks
    blocks[0].m_length = 5;
    blocks[0].m_instrs = allocate<Instr*>(5);
    blocks[1].m_length = 4;
    blocks[1].m_instrs = allocate<Instr*>(4);
    blocks[2].m_length = 2;
    blocks[2].m_instrs = allocate<Instr*>(2);
    blocks[3].m_length = 6;
    blocks[3].m_instrs = allocate<Instr*>(6);
    blocks[4].m_length = 8;
    blocks[4].m_instrs = allocate<Instr*>(8);

    // aio $4, $0
    // gc
    // ref $6, $5, "="
    // call %7, $6, <$0, $4>
    // tbr $7, $2, $1
    size_t call0[] = { 0, 4 };
    blocks[0].m_instrs[0] = allocInstr(AllocIntObjectInstr { 4, 0 });
    blocks[0].m_instrs[1] = allocInstr(GetContextInstr { 5 });
    blocks[0].m_instrs[2] = allocInstr(AccessInstr { 6, 5, "=" });
    blocks[0].m_instrs[3] = allocInstr(CallInstr { 7, 6, call0, 2 });
    blocks[0].m_instrs[4] = allocInstr(TestBranchInstr { 7, 1, 2 });

    // ref $8, $5, "+"
    // aio $9, $1
    // call $10, $8, <$1, $9>
    // ret $10
    size_t call1[] = { 1, 9 };
    blocks[1].m_instrs[0] = allocInstr(AccessInstr { 8, 5, "+" });
    blocks[1].m_instrs[1] = allocInstr(AllocIntObjectInstr { 9, 1 });
    blocks[1].m_instrs[2] = allocInstr(CallInstr { 10, 8, call1, 2});
    blocks[1].m_instrs[3] = allocInstr(ReturnInstr { 10 });

    // call $11, $6, <$1, $4>
    // tbr $11, $3, $4
    size_t call2[] = { 1, 4 };
    blocks[2].m_instrs[0] = allocInstr(CallInstr { 11, 6, call2, 2});
    blocks[2].m_instrs[1] = allocInstr(TestBranchInstr { 11, 3, 4 });

    // ref $12, $5, "-"
    // aio $13, $1
    // call $14, $12, <$0, $13>
    // ref $15, $5, "ack"
    // call $16, $15, <$14, $13>
    // ret $16
    size_t call3[] = { 0, 13 };
    size_t call4[] = { 14, 13 };
    blocks[3].m_instrs[0] = allocInstr(AccessInstr { 12, 5, "-" });
    blocks[3].m_instrs[1] = allocInstr(AllocIntObjectInstr { 13, 1 });
    blocks[3].m_instrs[2] = allocInstr(CallInstr { 14, 12, call3, 2});
    blocks[3].m_instrs[3] = allocInstr(AccessInstr { 15, 5, "ack" });
    blocks[3].m_instrs[4] = allocInstr(CallInstr { 16, 15, call4, 2});
    blocks[3].m_instrs[5] = allocInstr(ReturnInstr { 16 });

    // ref $17, $5, "-"
    // aio $18, $1
    // call $19, $17, <$0, $18>
    // call $20, $27, <$1, $18>
    // ref $21, $5, "ack"
    // call $22, $21, <$0, $20>
    // call $23, $23, <$19, $22>
    size_t call5[] = { 0, 18 };
    size_t call6[] = { 1, 18 };
    size_t call7[] = { 0, 20 };
    size_t call8[] = { 19, 22 };
    blocks[4].m_instrs[0] = allocInstr(AccessInstr { 17, 5, "-" });
    blocks[4].m_instrs[1] = allocInstr(AllocIntObjectInstr { 18, 1 });
    blocks[4].m_instrs[2] = allocInstr(CallInstr { 19, 17, call5, 2});
    blocks[4].m_instrs[3] = allocInstr(CallInstr { 20, 17, call6, 2});
    blocks[4].m_instrs[4] = allocInstr(AccessInstr { 21, 5, "ack" });
    blocks[4].m_instrs[5] = allocInstr(CallInstr { 22, 21, call7, 2});
    blocks[4].m_instrs[6] = allocInstr(CallInstr { 23, 21, call8, 2});
    blocks[4].m_instrs[7] = allocInstr(ReturnInstr { 23 });

    Object *ack = Object::newUserFunction(root, ackFn);
    root->set("ack", ack);

    // call it
    Object **args = allocate<Object *>(2);
    args[0] = Object::newInt(root, 3);
    args[1] = Object::newInt(root, 7);

    Object *result = handler(root, ack, args, 2);
    u::Log::out("ack(3, 7) = %d\n", ((IntObject *)result)->m_value);
}

}
