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
        bar = method() {
            print(this.a - this.b);
        }
    };

    obj.b = 7;
    obj["foo"] = method() {
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

// subclassing and object construction
let Class = { a = 0 };
let SubClass = new Class {
    b = 0,
    test = method() {
        print("a + b = ", this.a + this.b);
    }
};

let test = new SubClass;
test.a = 5;
test.b = 8;
test.test();

// allow subclassing primitive types, they become an instance of the primitive
// builtins so they can be coreced back to their primitive types whenever
let foo = new 5 { a = 7 };
print(foo); // prints 5

// operator overloads
let Overload = {
    a = 10,
    b = 40
};

Overload["+"] = method(other) {
    let value = {
        a = this.a + other.a,
        b = this.b + other.b
    };
    value["+"] = Overload["+"];
    return value;
};

let A = new Overload;
let B = new Overload;
let C = A + B; // 10+10, 40+40 => (20, 80)
let D = C + A; // 20+10, 80+40 => (30, 120)

print("a = ", D.a, ", B = ", D.b);

)";

void test() {
    Object *root = createRoot();
    char *text = script;

    UserFunction *module = Parser::parseModule(&text);
    module->dump();

    root = callFunction(root, module, nullptr, 0);

    GarbageCollector::run(root);
}

}
