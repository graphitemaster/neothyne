#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "s_parser.h"

#include "u_log.h"

namespace s {

void test() {
    Object *root = createRoot();

    char data[] =
    "fn level0() {"
    "  let obj = {"
    "    a = 5,"
    "    b = null,"
    "    bar = mfn() {"
    "      print(this.a - this.b);"
    "    }"
    "  };"
    "  obj.b = 7;"
    "  obj[\"foo\"] = mfn() { print(this.a + this.b); };"
    "  let a = 10;"
    "  while (a > 0) {"
    "    obj.foo();"
    "    obj.bar();"
    "    a = a - 1;"
    "  }"
    "}"
    "fn level1() { level0(); }"
    "fn level2() { level1(); }"
    "fn level3() { level2(); }"
    "let obj = { a = level3 };"
    "obj[\"a\"]();";
    char *text = data;

    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    GC::run();
}

}
