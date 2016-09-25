#ifndef S_RUNTIME_HDR
#define S_RUNTIME_HDR

#include <stddef.h>

namespace s {

struct Instr;
struct Object;
struct UserFunction;

// builtin functions
Object *equals(Object *context, Object *function, Object **args, size_t length);
Object *add(Object *context, Object *function, Object **args, size_t length);
Object *sub(Object *context, Object *function, Object **args, size_t length);
Object *mul(Object *context, Object *function, Object **args, size_t length);

// interpreter
Object *callFunction(Object *context, UserFunction *function, Object **args, size_t length);
Object *closureHandler(Object *context, Object *function, Object **args, size_t length);

// construct the "root" environment
Object *createRoot();

}

#endif
