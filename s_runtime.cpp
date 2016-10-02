#include "s_runtime.h"
#include "s_object.h"
#include "s_gc.h"

#include "u_log.h"
#include "u_misc.h"

namespace s {

enum { kAdd, kSub, kMul, kDiv };
enum { kEq, kLt, kGt, kLe, kGe };

/// [Bool]
static void boolNot(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)function;
    (void)arguments;
    U_ASSERT(count == 0);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;
    state->m_result = Object::newBool(state, !((BoolObject *)self)->m_value);
}

/// [Int]
static void intMath(State *state, Object *self, Object *, Object **arguments, size_t count, int op) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length -1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    U_ASSERT(intObj1);

    if (intObj2) {
        int value1 = ((IntObject *)intObj1)->m_value;
        int value2 = ((IntObject *)intObj2)->m_value;
        switch (op) {
        case kAdd: state->m_result = Object::newInt(state, value1 + value2); return;
        case kSub: state->m_result = Object::newInt(state, value1 - value2); return;
        case kMul: state->m_result = Object::newInt(state, value1 * value2); return;
        case kDiv: state->m_result = Object::newInt(state, value1 / value2); return;
        }
    }

    Object *floatBase = Object::lookup(root, "float", nullptr);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    if (floatObj2) {
        float value1 = ((IntObject *)intObj1)->m_value;
        float value2 = ((FloatObject *)floatObj2)->m_value;
        switch (op) {
        case kAdd: state->m_result = Object::newFloat(state, value1 + value2); return;
        case kSub: state->m_result = Object::newFloat(state, value1 - value2); return;
        case kMul: state->m_result = Object::newFloat(state, value1 * value2); return;
        case kDiv: state->m_result = Object::newFloat(state, value1 / value2); return;
        }
    }

    U_UNREACHABLE();
}

static void intAdd(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kAdd);
}

static void intSub(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kSub);
}

static void intMul(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kMul);
}

static void intDiv(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kDiv);
}

static void intCompare(State *state, Object *self, Object *, Object **arguments, size_t count, int compare) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length -1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    U_ASSERT(intObj1);

    if (intObj2) {
        int value1 = ((IntObject *)intObj1)->m_value;
        int value2 = ((IntObject *)intObj2)->m_value;
        switch (compare) {
        case kEq: state->m_result = Object::newBool(state, value1 == value2); return;
        case kLt: state->m_result = Object::newBool(state, value1 < value2); return;
        case kGt: state->m_result = Object::newBool(state, value1 > value2); return;
        case kLe: state->m_result = Object::newBool(state, value1 <= value2); return;
        case kGe: state->m_result = Object::newBool(state, value1 >= value2); return;
        }
    }

    Object *floatBase = Object::lookup(root, "float", nullptr);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    if (floatObj2) {
        float value1 = ((IntObject *)intObj1)->m_value;
        float value2 = ((FloatObject *)floatObj2)->m_value;
        switch (compare) {
        case kEq: state->m_result = Object::newBool(state, value1 == value2); return;
        case kLt: state->m_result = Object::newBool(state, value1 < value2); return;
        case kGt: state->m_result = Object::newBool(state, value1 > value2); return;
        case kLe: state->m_result = Object::newBool(state, value1 <= value2); return;
        case kGe: state->m_result = Object::newBool(state, value1 >= value2); return;
        }
    }

    U_UNREACHABLE();
}

static void intCompareEq(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intCompare(state, self, function, arguments, count, kEq);
}

static void intCompareLt(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intCompare(state, self, function, arguments, count, kLt);
}

static void intCompareGt(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intCompare(state, self, function, arguments, count, kGt);
}

static void intCompareLe(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intCompare(state, self, function, arguments, count, kLe);
}

static void intCompareGe(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intCompare(state, self, function, arguments, count, kGe);
}

/// [Float]
static void floatMath(State *state, Object *self, Object *, Object **arguments, size_t count, int op) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length -1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *floatBase = Object::lookup(root, "float", nullptr);

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    U_ASSERT(floatObj1);

    float value1 = ((FloatObject *)floatObj1)->m_value;
    float value2 = floatObj2 ? ((FloatObject *)floatObj2)->m_value : ((IntObject *)intObj2)->m_value;
    switch (op) {
    case kAdd: state->m_result = Object::newFloat(state, value1 + value2); return;
    case kSub: state->m_result = Object::newFloat(state, value1 - value2); return;
    case kMul: state->m_result = Object::newFloat(state, value1 * value2); return;
    case kDiv: state->m_result = Object::newFloat(state, value1 / value2); return;
    }

    U_UNREACHABLE();
}

static void floatAdd(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatMath(state, self, function, arguments, count, kAdd);
}

static void floatSub(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatMath(state, self, function, arguments, count, kSub);
}

static void floatMul(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatMath(state, self, function, arguments, count, kMul);
}

static void floatDiv(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatMath(state, self, function, arguments, count, kDiv);
}

static void floatCompare(State *state, Object *self, Object *, Object **arguments, size_t count, int compare) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length -1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *floatBase = Object::lookup(root, "float", nullptr);

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    U_ASSERT(floatObj1);

    float value1 = ((FloatObject *)floatObj1)->m_value;
    float value2 = floatObj2 ? ((FloatObject *)floatObj2)->m_value : ((IntObject *)intObj2)->m_value;
    switch (compare) {
    case kEq: state->m_result = Object::newBool(state, value1 == value2); return;
    case kLt: state->m_result = Object::newBool(state, value1 < value2); return;
    case kGt: state->m_result = Object::newBool(state, value1 > value2); return;
    case kLe: state->m_result = Object::newBool(state, value1 <= value2); return;
    case kGe: state->m_result = Object::newBool(state, value1 >= value2); return;
    }

    U_UNREACHABLE();
}

static void floatCompareEq(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatCompare(state, self, function, arguments, count, kEq);
}

static void floatCompareLt(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatCompare(state, self, function, arguments, count, kLt);
}

static void floatCompareGt(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatCompare(state, self, function, arguments, count, kGt);
}

static void floatCompareLe(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatCompare(state, self, function, arguments, count, kLe);
}

static void floatCompareGe(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    floatCompare(state, self, function, arguments, count, kGe);
}

/// [String]
static void stringAdd(State *state, Object *self, Object *, Object **arguments, size_t count) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length -1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *stringBase = Object::lookup(root, "string", nullptr);

    Object *strObj1 = Object::instanceOf(self, stringBase);
    Object *strObj2 = Object::instanceOf(*arguments, stringBase);

    U_ASSERT(strObj1 && strObj2);

    state->m_result = Object::newString(state,
        u::format("%s%s", ((StringObject *)strObj1)->m_value,
            ((StringObject *)strObj2)->m_value).c_str());
}

/// [Closure]
static void closureMark(State *state, Object *object) {
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = Object::lookup(root, "closure", nullptr);
    ClosureObject *closureObject = (ClosureObject*)Object::instanceOf(object, closureBase);
    if (closureObject)
        Object::mark(state, closureObject->m_context);
}

/// [Array]
static void arrayMark(State *state, Object *object) {
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;
    Object *arrayBase = Object::lookup(root, "array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(object, arrayBase);
    if (arrayObject) {
        for (int i = 0; i < arrayObject->m_length; i++)
            Object::mark(state, arrayObject->m_contents[i]);
    }
}

static void arrayResize(State *state, Object *self, Object *, Object **arguments, size_t count) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *arrayBase = Object::lookup(root, "array", nullptr);

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    U_ASSERT(arrayObject && intObject);

    int newSize = intObject->m_value;
    U_ASSERT(newSize >= 0);

    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * newSize);
    arrayObject->m_length = newSize;

    Object::setNormal(self, "length", Object::newInt(state, newSize));

    state->m_result = self;
}

static void arrayPush(State *state, Object *self, Object *, Object **arguments, size_t count) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *arrayBase = Object::lookup(root, "array", nullptr);

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    U_ASSERT(arrayObject);

    Object *value = *arguments;
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * ++arrayObject->m_length);
    arrayObject->m_contents[arrayObject->m_length - 1] = value;

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_result = self;
}

static void arrayPop(State *state, Object *self, Object *, Object **, size_t count) {
    U_ASSERT(count == 0);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *arrayBase = Object::lookup(root, "array", nullptr);

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    U_ASSERT(arrayObject);

    Object *result = arrayObject->m_contents[arrayObject->m_length - 1];
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * --arrayObject->m_length);

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_result = result;
}

static void arrayIndex(State *state, Object *self, Object *, Object **arguments, size_t count) {
    U_ASSERT(count == 1);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *arrayBase = Object::lookup(root, "array", nullptr);

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    if (intObject) {
        U_ASSERT(arrayObject);
        int index = intObject->m_value;
        U_ASSERT(index >= 0 && index < arrayObject->m_length);
        state->m_result = arrayObject->m_contents[index];
    } else {
        state->m_result = nullptr;
    }
}

static void arrayIndexAssign(State *state, Object *self, Object *, Object **arguments, size_t count) {
    U_ASSERT(count == 2);
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *arrayBase = Object::lookup(root, "array", nullptr);

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    U_ASSERT(arrayObject && intObject);

    int index = intObject->m_value;
    U_ASSERT(index >= 0 && index < arrayObject->m_length);
    arrayObject->m_contents[index] = arguments[1];
    state->m_result = nullptr;
}

static void print(State *state, Object *, Object *, Object **arguments, size_t count) {
    Object *root = state->m_stack[state->m_length - 1].m_context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = Object::lookup(root, "int", nullptr);
    Object *boolBase = Object::lookup(root, "bool", nullptr);
    Object *floatBase = Object::lookup(root, "float", nullptr);
    Object *stringBase = Object::lookup(root, "string", nullptr);

    for (size_t i = 0; i < count; i++) {
        Object *argument = arguments[i];
        Object *intObj = Object::instanceOf(argument, intBase);
        Object *boolObj = Object::instanceOf(argument, boolBase);
        Object *floatObj = Object::instanceOf(argument, floatBase);
        Object *stringObj = Object::instanceOf(argument, stringBase);
        if (intObj) {
            u::Log::out("%d", ((IntObject *)intObj)->m_value);
            continue;
        }
        if (boolObj) {
            u::Log::out(((BoolObject *)boolObj)->m_value ? "true" : "false");
            continue;
        }
        if (floatObj) {
            u::Log::out("%f", ((FloatObject *)floatObj)->m_value);
            continue;
        }
        if (stringObj) {
            u::Log::out("%s", ((StringObject *)stringObj)->m_value);
            continue;
        }
    }
    u::Log::out("\n");
    state->m_result = nullptr;
}

Object *createRoot(State *state) {
    Object *root = Object::newObject(state, nullptr);

    state->m_root = root;
    state->m_stack[state->m_length - 1].m_context = root;

    RootSet pinned;
    GC::addRoots(state, &root, 1, &pinned);

    // null
    Object::setNormal(root, "null", nullptr);

    // function
    Object *functionObject = Object::newObject(state, nullptr);
    functionObject->m_flags |= kNoInherit;
    Object::setNormal(root, "function", functionObject);

    // closure
    Object *closureObject = Object::newObject(state, nullptr);
    closureObject->m_flags |= kNoInherit;
    Object::setNormal(root, "closure", closureObject);
    Object *closureMarkObject = Object::newMark(state);
    ((MarkObject *)closureMarkObject)->m_mark = closureMark;
    Object::setNormal(closureObject, "mark", closureMarkObject);

    // bool
    Object *boolObject = Object::newObject(state, nullptr);
    boolObject->m_flags |= kNoInherit;
    Object::setNormal(root, "bool", boolObject);
    Object::setNormal(boolObject, "!", Object::newFunction(state, boolNot));

    // int
    Object *intObject = Object::newObject(state, nullptr);
    intObject->m_flags |= kNoInherit;
    Object::setNormal(root, "int", intObject);
    Object::setNormal(intObject, "+", Object::newFunction(state, intAdd));
    Object::setNormal(intObject, "-", Object::newFunction(state, intSub));
    Object::setNormal(intObject, "*", Object::newFunction(state, intMul));
    Object::setNormal(intObject, "/", Object::newFunction(state, intDiv));
    Object::setNormal(intObject, "==", Object::newFunction(state, intCompareEq));
    Object::setNormal(intObject, "<", Object::newFunction(state, intCompareLt));
    Object::setNormal(intObject, ">", Object::newFunction(state, intCompareGt));
    Object::setNormal(intObject, "<=", Object::newFunction(state, intCompareLe));
    Object::setNormal(intObject, ">=", Object::newFunction(state, intCompareGe));

    // float
    Object *floatObject = Object::newObject(state, nullptr);
    floatObject->m_flags |= kNoInherit;
    Object::setNormal(root, "float", floatObject);
    Object::setNormal(floatObject, "+", Object::newFunction(state, floatAdd));
    Object::setNormal(floatObject, "-", Object::newFunction(state, floatSub));
    Object::setNormal(floatObject, "*", Object::newFunction(state, floatMul));
    Object::setNormal(floatObject, "/", Object::newFunction(state, floatDiv));
    Object::setNormal(floatObject, "==", Object::newFunction(state, floatCompareEq));
    Object::setNormal(floatObject, "<", Object::newFunction(state, floatCompareLt));
    Object::setNormal(floatObject, ">", Object::newFunction(state, floatCompareGt));
    Object::setNormal(floatObject, "<=", Object::newFunction(state, floatCompareLe));
    Object::setNormal(floatObject, ">=", Object::newFunction(state, floatCompareGe));

    // string
    Object *stringObject = Object::newObject(state, nullptr);
    stringObject->m_flags |= kNoInherit;
    Object::setNormal(root, "string", stringObject);
    Object::setNormal(stringObject, "+", Object::newFunction(state, stringAdd));

    // array
    Object *arrayObject = Object::newObject(state, nullptr);
    arrayObject->m_flags |= kNoInherit;
    Object::setNormal(root, "array", arrayObject);
    Object *arrayMarkObject = Object::newMark(state);
    ((MarkObject *)arrayMarkObject)->m_mark = arrayMark;
    Object::setNormal(arrayObject, "mark", arrayMarkObject);
    Object::setNormal(arrayObject, "resize", Object::newFunction(state, arrayResize));
    Object::setNormal(arrayObject, "push", Object::newFunction(state, arrayPush));
    Object::setNormal(arrayObject, "pop", Object::newFunction(state, arrayPop));
    Object::setNormal(arrayObject, "[]", Object::newFunction(state, arrayIndex));
    Object::setNormal(arrayObject, "[]=", Object::newFunction(state, arrayIndexAssign));

    // others
    Object::setNormal(root, "print", Object::newFunction(state, print));

    GC::delRoots(state, &pinned);

    return root;
}

}
