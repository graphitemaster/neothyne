#ifndef S_OPTIMIZE_HDR
#define S_OPTIMIZE_HDR

namespace s {

struct UserFunction;

struct Optimize {
    static UserFunction *predictPass(UserFunction *function);
    static UserFunction *fastSlotPass(UserFunction *function);
    static UserFunction *inlinePass(UserFunction *function);
};

}

#endif
