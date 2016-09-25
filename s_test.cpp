#include "s_runtime.h"
#include "s_object.h"
#include "s_instr.h"
#include "s_codegen.h"

#include "s_parser.h"

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
    char data[] =
    "fn ack(m, n) {"
    "   if (m == 0) return n + 1;"
    "   if (n == 0) return ack(m - 1, 1);"
    "   return ack(m - 1, ack(m, n - 1));"
    "}"
    "print(3, \", hello world, \", 3.14);";

    char *text = data;

    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    Object *ack = root->m_table.lookup("ack");

    // call it
    Object **args = allocate<Object *>(2);
    args[0] = Object::newInt(root, 3);
    args[1] = Object::newInt(root, 7);

    Object *result = closureHandler(root, ack, args, 2);
    u::Log::out("ack(3, 7) = %d\n", ((IntObject *)result)->m_value);
}

}
