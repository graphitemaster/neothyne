#include "s_parser.h"
#include "s_object.h"
#include "s_runtime.h"
#include "s_vm.h"
#include "s_gc.h"
#include "s_memory.h"

#include "u_log.h"

namespace s {

char text[] = R"(

// Hofstadter-Conway $10,000 sequence
fn eval(m) {
    let k1 = 2;
    let lg2 = 1;
    let amax = 0.0;
    let list = [ -50000, 1, 1 ].resize(0x100000 + 1);
    let v = list[2];
    for (let n = 3; n <= m; n++) {
        list[n] = list[v] + list[n-v];
        v = list[n];
        if (amax < v*1.0/n) amax = v*1.0/n;
        if (0 == (k1&n)) {
            print("Maximum between 2^", lg2, " and 2^", lg2+1, " was ", amax, "\n");
            amax = 0;
            lg2++;
        }
        k1 = n;
    }
}
eval(0x100000);

// Thiele's interpolation formula
const N = 32;
const N2 = (N * (N - 1) / 2);
const STEP = 0.05;

let xval = [].resize(N);
let tsin = [].resize(N);
let tcos = [].resize(N);
let ttan = [].resize(N);
let rsin = [].resize(N2);
let rcos = [].resize(N2);
let rtan = [].resize(N2);

fn rho(x, y, r, i, n) {
    if (n < 0) return 0;
    if (n == 0) return y[i];
    let idx = (N - 1 - n) * (N - n) / 2 + i;
    if (r[idx] != r[idx])
        r[idx] = (x[i] - x[i + n]) / (rho(x, y, r, i, n - 1) - rho(x, y, r, i + 1, n - 1)) + rho(x, y, r, i + 1, n - 2);
    return r[idx];
}

fn thiele(x, y, r, xin, n) {
    if (n > N - 1) return 1;
    return rho(x, y, r, 0, n) - rho(x, y, r, 0, n - 2)
        + (xin - x[n]) / thiele(x, y, r, xin, n + 1);
}

fn isin(x) { return thiele(tsin, xval, rsin, x, 0); }
fn icos(x) { return thiele(tcos, xval, rcos, x, 0); }
fn itan(x) { return thiele(ttan, xval, rtan, x, 0); }

for (let i = 0; i < N; i++) {
    xval[i] = i * STEP;
    tsin[i] = Math.sin(xval[i]);
    tcos[i] = Math.cos(xval[i]);
    ttan[i] = Math.tan(xval[i]); // optionally approximate with division: tsin[i] / tcos[i];
}

for (let i = 0; i < N2; i++) {
    rsin[i] = 0.0/0.0;
    rcos[i] = 0.0/0.0;
    rtan[i] = 0.0/0.0;
}

print(6. * isin(.5), "\n");
print(3. * icos(.5), "\n");
print(4. * itan(1.), "\n");

// Ackermann function
fn ackermann(m, n) {
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

for (let m = 0; m <= 5; m++)
    for (let n = 0; n < 5 - m; n++)
        print("A(", m, ", ", n, ") = ", ackermann(m, n), "\n");

// Matrix transposition
fn transpose(d, s, h, w) {
    for (let i = 0; i < h; i++)
        for (let j = 0; j < w; j++)
            d[j][i] = s[i][j];
}

let a = [ [ 0, 1, 2, 3, 4 ],
          [ 5, 6, 7, 8, 9 ],
          [ 1, 0, 0, 0, 42 ] ];

let b = [].resize(5);
for (let i = 0; i < 5; i++)
    b[i] = [].resize(3);

transpose(b, a, 3, 5);

for (let i = 0; i < 5; i++) {
    for (let j = 0; j < 3; j++) {
        print(b[i][j]);
        if (j == 2) print("\n"); else print(" ");
    }
}

// N-queens (10 in this case) (724 solutions)
const N = 10;
let solution = 0;
let position = [].resize(N);

fn is_safe(queen, row) {
    for (let i = 0; i < queen; i++) {
        let other = position[i];
        if (other == row) return 0; // same row
        if (other == row - (queen - i)) return 0; // same diagonal
        if (other == row + (queen - i)) return 0; // same diagonal
    }
    return 1;
}

fn solve(k) {
    if (k == N) { // placed N-1 queens (0 included)
        solution++;
        print("Solution ", solution, ": ");
        for (let i = 0; i < N; i++)
            print(position[i], " ");
        print("\n");
    } else {
        for (let i = 0; i < N; i++) {
            // before putting a queen (the k-th) into a row, check if it's safe
            if (is_safe(k, i) == 1) {
                position[k] = i;
                // go again
                solve(k + 1);
            }
        }
    }
}

solve(0); // kick it off


)";

void test() {
    // Allocate memory for Neo
    Memory::init();

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
    UserFunction *module = nullptr;
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

    // Reclaim shared state
    neoFree(state.m_shared);

    // Tear down any stack frames made
    neoFree((CallFrame *)state.m_stack - 1);

    // Reclaim any leaking memory
    Memory::destroy();
}

}
