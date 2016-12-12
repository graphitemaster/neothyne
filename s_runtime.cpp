#include "s_runtime.h"
#include "s_object.h"
#include "s_vm.h"
#include "s_gc.h"

#include "u_log.h"
#include "u_misc.h"

#include "m_trig.h"

namespace s {

enum { kAdd, kSub, kMul, kDiv, kBitOr, kBitAnd };
enum { kEq, kLt, kGt, kLe, kGe };

/// [Bool]
static void boolNot(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)function;
    (void)arguments;
    VM_ASSERT(count == 0, "wrong number of parameters: expected 0, got %zu", count);
    state->m_resultValue = Object::newBool(state, !((BoolObject *)self)->m_value);
}

/// [Int]
static void intMath(State *state, Object *self, Object *, Object **arguments, size_t count, int op) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    VM_ASSERT(intObj1, "wrong type: expected int");

    if (intObj2) {
        int value1 = ((IntObject *)intObj1)->m_value;
        int value2 = ((IntObject *)intObj2)->m_value;
        switch (op) {
        case kAdd:    state->m_resultValue = Object::newInt(state, value1 + value2); return;
        case kSub:    state->m_resultValue = Object::newInt(state, value1 - value2); return;
        case kMul:    state->m_resultValue = Object::newInt(state, value1 * value2); return;
        case kDiv:    state->m_resultValue = Object::newInt(state, value1 / value2); return;
        case kBitAnd: state->m_resultValue = Object::newInt(state, value1 & value2); return;
        case kBitOr:  state->m_resultValue = Object::newInt(state, value1 | value2); return;
        }
    }

    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    if (floatObj2) {
        float value1 = ((IntObject *)intObj1)->m_value;
        float value2 = ((FloatObject *)floatObj2)->m_value;
        switch (op) {
        case kAdd: state->m_resultValue = Object::newFloat(state, value1 + value2); return;
        case kSub: state->m_resultValue = Object::newFloat(state, value1 - value2); return;
        case kMul: state->m_resultValue = Object::newFloat(state, value1 * value2); return;
        case kDiv: state->m_resultValue = Object::newFloat(state, value1 / value2); return;
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

static void intBitAnd(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kBitAnd);
}

static void intBitOr(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    intMath(state, self, function, arguments, count, kBitOr);
}

static void intCompare(State *state, Object *self, Object *, Object **arguments, size_t count, int compare) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    VM_ASSERT(intObj1, "wrong type: expected int");

    if (intObj2) {
        int value1 = ((IntObject *)intObj1)->m_value;
        int value2 = ((IntObject *)intObj2)->m_value;
        switch (compare) {
        case kEq: state->m_resultValue = Object::newBool(state, value1 == value2); return;
        case kLt: state->m_resultValue = Object::newBool(state, value1 < value2); return;
        case kGt: state->m_resultValue = Object::newBool(state, value1 > value2); return;
        case kLe: state->m_resultValue = Object::newBool(state, value1 <= value2); return;
        case kGe: state->m_resultValue = Object::newBool(state, value1 >= value2); return;
        }
    }

    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    if (floatObj2) {
        float value1 = ((IntObject *)intObj1)->m_value;
        float value2 = ((FloatObject *)floatObj2)->m_value;
        switch (compare) {
        case kEq: state->m_resultValue = Object::newBool(state, value1 == value2); return;
        case kLt: state->m_resultValue = Object::newBool(state, value1 < value2); return;
        case kGt: state->m_resultValue = Object::newBool(state, value1 > value2); return;
        case kLe: state->m_resultValue = Object::newBool(state, value1 <= value2); return;
        case kGe: state->m_resultValue = Object::newBool(state, value1 >= value2); return;
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
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    VM_ASSERT(floatObj1, "wrong type: expected float");

    float value1 = ((FloatObject *)floatObj1)->m_value;
    float value2 = floatObj2 ? ((FloatObject *)floatObj2)->m_value : ((IntObject *)intObj2)->m_value;
    switch (op) {
    case kAdd: state->m_resultValue = Object::newFloat(state, value1 + value2); return;
    case kSub: state->m_resultValue = Object::newFloat(state, value1 - value2); return;
    case kMul: state->m_resultValue = Object::newFloat(state, value1 * value2); return;
    case kDiv: state->m_resultValue = Object::newFloat(state, value1 / value2); return;
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
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    VM_ASSERT(floatObj1, "wrong type: expected float");

    float value1 = ((FloatObject *)floatObj1)->m_value;
    float value2 = floatObj2 ? ((FloatObject *)floatObj2)->m_value : ((IntObject *)intObj2)->m_value;
    switch (compare) {
    case kEq: state->m_resultValue = Object::newBool(state, value1 == value2); return;
    case kLt: state->m_resultValue = Object::newBool(state, value1 < value2); return;
    case kGt: state->m_resultValue = Object::newBool(state, value1 > value2); return;
    case kLe: state->m_resultValue = Object::newBool(state, value1 <= value2); return;
    case kGe: state->m_resultValue = Object::newBool(state, value1 >= value2); return;
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
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    Object *strObj1 = Object::instanceOf(self, stringBase);
    Object *strObj2 = Object::instanceOf(*arguments, stringBase);

    VM_ASSERT(strObj1 && strObj2, "wrong type: expected string");

    state->m_resultValue = Object::newString(state,
                                             u::format("%s%s",
                                                       ((StringObject *)strObj1)->m_value,
                                                       ((StringObject *)strObj2)->m_value).c_str());
}

static void stringCompare(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    Object *strObj1 = Object::instanceOf(self, stringBase);
    Object *strObj2 = Object::instanceOf(*arguments, stringBase);

    VM_ASSERT(strObj1 && strObj2, "wrong type: expected string");

    const char *str1 = ((StringObject *)strObj1)->m_value;
    const char *str2 = ((StringObject *)strObj2)->m_value;

    state->m_resultValue = Object::newBool(state, strcmp(str1, str2) == 0);
}

/// [Closure]
static void closureMark(State *state, Object *object) {
    Object *closureBase = state->m_shared->m_valueCache.m_closureBase;
    ClosureObject *closureObject = (ClosureObject*)Object::instanceOf(object, closureBase);
    if (closureObject) {
        Object::mark(state, closureObject->m_context);
    }
}

/// [Array]
static void arrayMark(State *state, Object *object) {
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(object, arrayBase);
    if (arrayObject) {
        for (int i = 0; i < arrayObject->m_length; i++) {
            Object::mark(state, arrayObject->m_contents[i]);
        }
    }
}

static void arrayResize(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    VM_ASSERT(arrayObject, "wrong type: expected array");
    VM_ASSERT(intObject, "wrong type: expected int");

    int oldSize = arrayObject->m_length;
    int newSize = intObject->m_value;

    VM_ASSERT(newSize >= 0, "'array.resize(%d)' not allowed", newSize);

    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * newSize);
    memset(arrayObject->m_contents + oldSize, 0, sizeof(Object *) * (newSize - oldSize));
    arrayObject->m_length = newSize;

    Object::setNormal(self, "length", Object::newInt(state, newSize));

    state->m_resultValue = self;
}

static void arrayPush(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    VM_ASSERT(arrayObject, "wrong type: expected array");

    Object *value = *arguments;
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * ++arrayObject->m_length);
    arrayObject->m_contents[arrayObject->m_length - 1] = value;

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_resultValue = self;
}

static void arrayPop(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT(count == 0, "wrong number of parameters: expected 0, got %zu", count);

    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    VM_ASSERT(arrayObject, "wrong type: expected array");

    Object *result = arrayObject->m_contents[arrayObject->m_length - 1];
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * --arrayObject->m_length);

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_resultValue = result;
}

static void arrayIndex(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    if (intObject) {
        VM_ASSERT(arrayObject, "wrong type: expected array");
        const int index = intObject->m_value;
        VM_ASSERT(index >= 0 && index < arrayObject->m_length, "index out of range");
        state->m_resultValue = arrayObject->m_contents[index];
    } else {
        state->m_resultValue = nullptr;
    }
}

static void arrayIndexAssign(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 2, "wrong number of parameters: expected 2, got %zu", count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    VM_ASSERT(arrayObject, "wrong type: expected array");
    VM_ASSERT(intObject, "wrong type: expected int");

    const int index = intObject->m_value;
    VM_ASSERT(index >= 0 && index < arrayObject->m_length, "index out of range");
    arrayObject->m_contents[index] = arguments[1];
    state->m_resultValue = nullptr;
}

static void print(State *state, Object *, Object *, Object **arguments, size_t count) {
    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *boolBase = state->m_shared->m_valueCache.m_boolBase;
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

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
    state->m_resultValue = nullptr;
}

enum { kSin, kCos, kTan, kSqrt };

static void mathTrig(State *state, Object *, Object **arguments, size_t count, int type) {
    VM_ASSERT(count == 1, "wrong number of parameters: expected 1, got %zu", count);
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    FloatObject *floatObject = (FloatObject *)Object::instanceOf(*arguments, floatBase);
    VM_ASSERT(floatObject, "wrong type: expected float");
    switch (type) {
    case kSin:  state->m_resultValue = Object::newFloat(state, m::sin(floatObject->m_value));  break;
    case kCos:  state->m_resultValue = Object::newFloat(state, m::cos(floatObject->m_value));  break;
    case kTan:  state->m_resultValue = Object::newFloat(state, m::tan(floatObject->m_value));  break;
    case kSqrt: state->m_resultValue = Object::newFloat(state, m::sqrt(floatObject->m_value)); break;
    }
}

static void mathSin(State *state, Object *self, Object *, Object **arguments, size_t count) {
    return mathTrig(state, self, arguments, count, kSin);
}

static void mathCos(State *state, Object *self, Object *, Object **arguments, size_t count) {
    return mathTrig(state, self, arguments, count, kCos);
}

static void mathTan(State *state, Object *self, Object *, Object **arguments, size_t count) {
    return mathTrig(state, self, arguments, count, kTan);
}

static void mathSqrt(State *state, Object *self, Object *, Object **arguments, size_t count) {
    return mathTrig(state, self, arguments, count, kSqrt);
}

static void mathPow(State *state, Object *, Object *, Object **arguments, size_t count) {
    VM_ASSERT(count == 2, "wrong number of parameters: expected 2, got %zu", count);
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    FloatObject *lhsObject = (FloatObject *)Object::instanceOf(arguments[0], floatBase);
    FloatObject *rhsObject = (FloatObject *)Object::instanceOf(arguments[1], floatBase);
    VM_ASSERT(lhsObject && rhsObject, "wrong type: expected float");
    state->m_resultValue = Object::newFloat(state, m::pow(lhsObject->m_value, rhsObject->m_value));
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
    state->m_shared->m_valueCache.m_functionBase = functionObject;
    functionObject->m_flags |= kNoInherit;
    Object::setNormal(root, "function", functionObject);
    functionObject->m_flags |= kImmutable;

    // closure
    Object *closureObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_closureBase = closureObject;
    closureObject->m_flags |= kNoInherit;
    Object::setNormal(root, "closure", closureObject);
    closureObject->m_mark = closureMark;

    // bool
    Object *boolObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_boolBase = boolObject;
    boolObject->m_flags |= kNoInherit;
    Object::setNormal(root, "bool", boolObject);
    Object::setNormal(boolObject, "!", Object::newFunction(state, boolNot));
    Object *trueObject = Object::newBoolUncached(state, true);
    Object *falseObject = Object::newBoolUncached(state, false);
    Object::setNormal(root, "true", trueObject);
    Object::setNormal(root, "false", falseObject);
    state->m_shared->m_valueCache.m_boolTrue = trueObject;
    state->m_shared->m_valueCache.m_boolFalse = falseObject;
    boolObject->m_flags |= kImmutable;

    // int
    Object *intObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_intBase = intObject;
    intObject->m_flags |= kNoInherit;
    Object::setNormal(root, "int", intObject);
    Object::setNormal(intObject, "+", Object::newFunction(state, intAdd));
    Object::setNormal(intObject, "-", Object::newFunction(state, intSub));
    Object::setNormal(intObject, "*", Object::newFunction(state, intMul));
    Object::setNormal(intObject, "/", Object::newFunction(state, intDiv));
    Object::setNormal(intObject, "&", Object::newFunction(state, intBitAnd));
    Object::setNormal(intObject, "|", Object::newFunction(state, intBitOr));
    Object::setNormal(intObject, "==", Object::newFunction(state, intCompareEq));
    Object::setNormal(intObject, "<", Object::newFunction(state, intCompareLt));
    Object::setNormal(intObject, ">", Object::newFunction(state, intCompareGt));
    Object::setNormal(intObject, "<=", Object::newFunction(state, intCompareLe));
    Object::setNormal(intObject, ">=", Object::newFunction(state, intCompareGe));
    state->m_shared->m_valueCache.m_intZero = Object::newInt(state, 0);
    GC::addPermanent(state, state->m_shared->m_valueCache.m_intZero);
    intObject->m_flags |= kImmutable;

    // float
    Object *floatObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_floatBase = floatObject;
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
    floatObject->m_flags |= kImmutable;

    // string
    Object *stringObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_stringBase = stringObject;
    stringObject->m_flags |= kNoInherit;
    Object::setNormal(root, "string", stringObject);
    Object::setNormal(stringObject, "+", Object::newFunction(state, stringAdd));
    Object::setNormal(stringObject, "==", Object::newFunction(state, stringCompare));
    stringObject->m_flags |= kImmutable;

    // array
    Object *arrayObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_arrayBase = arrayObject;
    arrayObject->m_flags |= kNoInherit;
    Object::setNormal(root, "array", arrayObject);
    arrayObject->m_mark = arrayMark;
    Object::setNormal(arrayObject, "resize", Object::newFunction(state, arrayResize));
    Object::setNormal(arrayObject, "push", Object::newFunction(state, arrayPush));
    Object::setNormal(arrayObject, "pop", Object::newFunction(state, arrayPop));
    Object::setNormal(arrayObject, "[]", Object::newFunction(state, arrayIndex));
    Object::setNormal(arrayObject, "[]=", Object::newFunction(state, arrayIndexAssign));
    arrayObject->m_flags |= kClosed | kImmutable;

    // others
    Object::setNormal(root, "print", Object::newFunction(state, print));

    // Math
    Object *mathObject = Object::newObject(state, nullptr);
    mathObject->m_flags |= kNoInherit | kImmutable;
    Object::setNormal(root, "Math", mathObject);
    Object::setNormal(mathObject, "sin", Object::newFunction(state, mathSin));
    Object::setNormal(mathObject, "cos", Object::newFunction(state, mathCos));
    Object::setNormal(mathObject, "tan", Object::newFunction(state, mathTan));
    Object::setNormal(mathObject, "sqrt", Object::newFunction(state, mathSqrt));
    Object::setNormal(mathObject, "pow", Object::newFunction(state, mathPow));
    mathObject->m_flags |= kClosed;

    GC::delRoots(state, &pinned);

    return root;
}

}
