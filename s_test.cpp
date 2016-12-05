#include "s_parser.h"
#include "s_object.h"
#include "s_runtime.h"
#include "s_vm.h"
#include "s_gc.h"

#include "u_log.h"

namespace s {

char text[] = R"(
fn foo(oh) {
    if (oh > 10) {
        let s1 = 0;
        let s2 = 1;
        let a = 0;
        while (a < 100) {
            let b = 0;
            while (b < 1000) {
                s1 = s1 + 1;
                s2 = s2 * s1;
                b = b + 1;
            }
            a = a + 1;
        }
        print(s);
    } else {
        return foo(oh + 1);
    }
    return 1;
}
foo(0);
)";

void test() {
    State state;
    memset(&state, 0, sizeof state);
    state.m_gc = (GCState *)neoCalloc(sizeof *state.m_gc, 1);
    state.m_profileState = (ProfileState *)neoCalloc(sizeof *state.m_profileState, 1);

    VM::addFrame(&state, 0);
    Object *root = createRoot(&state);
    VM::delFrame(&state);

    char *contents = text;

    RootSet set;
    GC::addRoots(&state, &root, 1, &set);

    SourceRange source;
    source.m_begin = contents;
    source.m_end = contents + sizeof text;
    SourceRecord::registerSource(source, "test", 0, 0);

    UserFunction *module;
    char *text = source.m_begin;
    ParseResult result = Parser::parseModule(&text, &module);
    (void)result;
    if (result == kParseOk) {
        //U_ASSERT(result == kParseOk);
        //UserFunction::dump(module, 0);

        VM::callFunction(&state, root, module, nullptr, 0);
        VM::run(&state);

        // dump the profile
        ProfileState::dump(source, state.m_profileState);

        // print any errors from running
        if (state.m_runState == kErrored)
            u::Log::err("%s\n", state.m_error);
    }

    GC::delRoots(&state, &set);
    GC::run(&state);

    neoFree(module);
}

}
