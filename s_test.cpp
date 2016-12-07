#include "s_parser.h"
#include "s_object.h"
#include "s_runtime.h"
#include "s_vm.h"
#include "s_gc.h"

#include "u_log.h"

namespace s {

char text[] = R"(
fn ack(m, n) {
    if (m == 0) return n + 1;
    if (n == 0) return ack(m - 1, 1);
    return ack(m - 1, ack(m, n - 1));
}

ack(3, 4);

for (let i = 0; i < 4096; i++) {
    print(i);
}

fn factorial(x) {
    if (x == 1) return x;
    return x * factorial(x - 1);
}

factorial(4096);

fn mod(a, b) {
    return a-(a/b*b);
}

let i = 0;
let j = 1;
while (i < 2) {
    print(mod(i, j));
    i = i + 1;
    j = j + i;
}


)";

void test() {
    // Allocate Neo state
    State state = { };
    state.m_shared = (SharedState *)neoCalloc(sizeof *state.m_shared, 1);

    // Initialize garbage collector
    GC::init(&state);

    // Create a frame on the VM to execute the root object construction
    VM::addFrame(&state, 0);
    Object *root = createRoot(&state);
    VM::delFrame(&state);

    char *contents = text;

    // Create the pinning set for the GC
    RootSet set;
    GC::addRoots(&state, &root, 1, &set);

    // Specify the source contents
    SourceRange source;
    source.m_begin = contents;
    source.m_end = contents + sizeof text;
    SourceRecord::registerSource(source, "test", 0, 0);

    // Parse the result into our module
    UserFunction *module;
    char *text = source.m_begin;
    ParseResult result = Parser::parseModule(&text, &module);
    (void)result;
    if (result == kParseOk) {
        // Execute the result on the VM
        VM::callFunction(&state, root, module, nullptr, 0);
        VM::run(&state);

        // Export the profile of the code
        ProfileState::dump(source, &state.m_shared->m_profileState);

        // Did we error out while running?
        if (state.m_runState == kErrored) {
            u::Log::err("%s\n", state.m_error);
        }
    }

    // Tear down the objects represented by this set
    GC::delRoots(&state, &set);

    // Reclaim memory
    GC::run(&state);

    // Destroy the function
    neoFree(module);
}

}
