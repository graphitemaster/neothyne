#include "s_parser.h"
#include "s_object.h"
#include "s_runtime.h"
#include "s_vm.h"
#include "s_gc.h"

#include "u_log.h"

namespace s {

char text[] = R"(

let obj1 = {
    a = 5,
    b = null,
    bar = method() {
        print("obj1 = ", this.a - this.b);
    }
};
obj1.b = 7;
obj1.bar();

// shadows the b inside obj1
let obj2 = new obj1 { b = 9 };
print("obj2.b = ", obj2.b);

// prototype chain lookup for 5
let obj3 = new 5 { bar = 7 };
print("obj3 = ", obj3 + obj3.bar);

let a = [2, 3, 4];
a[1] = 7; // replace 3 with 7
print(a[0], a[1], a[2]);
let t = a.push(10).push(20).pop();
print(t);
print(a[0], a[1], a[2], a[3]);

fn ack(m, n) {
    let np = n + 1, nm = n - 1, mm = m - 1;
    if (m < .5) return np;
    if (n == 0) return ack(mm, 1);
    return ack(mm, ack(m, nm));
}
print(ack(3, 7));

print("string " + "concatenation");

)";

void test() {
    State state;
    memset(&state, 0, sizeof state);
    VM::addFrame(&state);

    Object *root = createRoot(&state);

    char *contents = text;

    RootSet set;
    GC::addRoots(&state, &root, 1, &set);

    UserFunction *module = Parser::parseModule(&contents);
    UserFunction::dump(module, 0);

    state.m_length = 0;
    VM::callFunction(&state, root, module, nullptr, 0);
    VM::run(&state, root);

    root = state.m_result;

    GC::delRoots(&state, &set);
    GC::run(&state);

    neoFree(module);
}

}
