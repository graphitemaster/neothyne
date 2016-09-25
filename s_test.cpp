#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "s_parser.h"

#include "u_log.h"

namespace s {

static char script[] = R"(
fn level0() {
    let obj = {
        a = 5,
        b = null,
        bar = mfn() {
            print(this.a - this.b);
        }
    };

    obj.b = 7;
    obj["foo"] = mfn() {
        print(this.a + this.b);
    };

    let a = 10;
    while (a > 0) {
        obj.foo();
        obj.bar();
        a = a - 1;
    }
}

fn level1() { level0(); }
fn level2() { level1(); }
fn level3() { level2(); }

let obj = {
    a = level3
};

obj["a"]();

print("2 != 2 => ", 2 != 2);
print("2 !< 2 => ", 2 !< 2);
print("2 !> 2 => ", 2 !> 2);
print("2 !<= 2 => ", 2 !<= 2);
print("2 !>= 2 => ", 2 !>= 2);

print("string " + "concatenation");

let A = { value = 1337 };
let B = { value = A };
let C = { value = B };
let D = { value = C };

print(D.value.value.value.value);
print(D["value"]["value"]["value"]["value"]);

// comment

/* a multi
   line comment
   to test this */

// make cycles to test GC
let tree = { next = null, prev = null };
tree.next = tree;
tree.prev = tree;

)";

void test() {
    Object *root = createRoot();
    char *text = script;

    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    GC::run();
}

}
