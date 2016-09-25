#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "s_parser.h"

#include "u_log.h"

namespace s {

void test() {
    Object *root = createRoot();

    char data[] =
    "let obj = {"
    "  a = 5,"
    "  b = null,"
    "  bar = mfn() {"
    "    print(this.a - this.b);"
    "  }"
    "};"
    "obj.b = 7;"
    "obj[\"foo\"] = mfn() { print(this.a + this.b); };"
    "obj.foo();"
    "obj.bar();";
    char *text = data;

    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    GC::run();
}

}
