#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "s_parser.h"

#include "u_log.h"

namespace s {

void test() {
    Object *root = createRoot();
    //void *entry = GC::addRoots(&root, 1);

    char data[] =
    "let obj = { a = 5 };"
    "print(obj.a * 2);";

    char *text = data;
    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    //GC::removeRoots(entry);
    GC::run();
}

}
