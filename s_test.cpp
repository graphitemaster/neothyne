#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "s_parser.h"

#include "u_log.h"

namespace s {

void test() {
    Object *root = createRoot();
    void *entry = GC::addRoots(&root, 1);

    char data[] =
    "let obj = { a = 5 };"
    "fn ack(m, n) {"
    "   if (m == 0) return n + 1;"
    "   if (n == 0) return ack(m - 1, 1);"
    "   return ack(m - 1, ack(m, n - 1));"
    "}"
    "print(obj.a);";

    char *text = data;
    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    Object *ack = root->m_table.lookup("ack");

    // call it
    Object **args = allocate<Object *>(2);
    args[0] = Object::newInt(root, 2);
    args[1] = Object::newInt(root, 3);

    Object *result = functionHandler(root, nullptr, ack, args, 2);
    u::Log::out("ack(2, 3) = %d\n", ((IntObject *)result)->m_value);

    GC::removeRoots(entry);
    GC::run();
}

}
