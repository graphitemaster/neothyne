#include "s_runtime.h"
#include "s_object.h"
#include "s_memory.h"
#include "s_parser.h"
#include "s_vm.h"
#include "s_gc.h"

#include "u_log.h"
#include "u_misc.h"

#include "m_trig.h"

#include "engine.h" // neoGamePath()

namespace s {

enum { kAdd, kSub, kMul, kDiv, kBitOr, kBitAnd };
enum { kEq, kLt, kGt, kLe, kGe };

/// [Bool]
static void boolNot(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)function;
    (void)arguments;

    VM_ASSERT_ARITY(0_z, count);

    state->m_resultValue = Object::newBool(state, !((BoolObject *)self)->m_value);
}

static void boolCmp(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)function;
    (void)arguments;

    VM_ASSERT_ARITY(1_z, count);

    Object *boolBase = state->m_shared->m_valueCache.m_boolBase;

    auto *boolObj1 = (BoolObject *)Object::instanceOf(self, boolBase);
    auto *boolObj2 = (BoolObject *)Object::instanceOf(*arguments, boolBase);

    VM_ASSERT_TYPE(boolObj1, "Bool");

    state->m_resultValue = Object::newBool(state, boolObj1->m_value == boolObj2->m_value);
}

/// [Int]
static void intMath(State *state, Object *self, Object *, Object **arguments, size_t count, int op) {
    VM_ASSERT_ARITY(1_z, count);
    VM_ASSERT(arguments[0], "cannot perform integer arithmetic on Null");

    Object *intBase = state->m_shared->m_valueCache.m_intBase;

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    VM_ASSERT_TYPE(intObj1, "Int");

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
        case kAdd:    state->m_resultValue = Object::newFloat(state, value1 + value2); return;
        case kSub:    state->m_resultValue = Object::newFloat(state, value1 - value2); return;
        case kMul:    state->m_resultValue = Object::newFloat(state, value1 * value2); return;
        case kDiv:    state->m_resultValue = Object::newFloat(state, value1 / value2); return;
        case kBitAnd: VM_ASSERT(false, "bit and with float operand not supported"); return;
        case kBitOr:  VM_ASSERT(false, "bit or with float operand not supported"); return;
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
    VM_ASSERT_ARITY(1_z, count);
    VM_ASSERT(arguments[0], "cannot compare Int with Null");

    Object *intBase = state->m_shared->m_valueCache.m_intBase;

    Object *intObj1 = Object::instanceOf(self, intBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);

    VM_ASSERT_TYPE(intObj1, "Int");

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

static void intToFloat(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT_ARITY(0_z, count);

    auto *intBase = state->m_shared->m_valueCache.m_intBase;
    auto *intObj1 = (IntObject *)Object::instanceOf(self, intBase);

    state->m_resultValue = Object::newFloat(state, intObj1->m_value);
}

static void intToString(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT_ARITY(0_z, count);

    auto *intBase = state->m_shared->m_valueCache.m_intBase;
    auto *intObj1 = (IntObject *)Object::instanceOf(self, intBase);

    char format[1024];
    snprintf(format, sizeof format, "%d", intObj1->m_value);
    state->m_resultValue = Object::newString(state, format, strlen(format));
}

/// [Float]
static void floatMath(State *state, Object *self, Object *, Object **arguments, size_t count, int op) {
    VM_ASSERT_ARITY(1_z, count);
    VM_ASSERT(arguments[0], "cannot perform floating point arithmetic on Null");

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    VM_ASSERT_TYPE(floatObj1, "Float");

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
    VM_ASSERT_ARITY(1_z, count);
    VM_ASSERT(arguments[0], "cannot compare Float with Null");

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;

    Object *floatObj1 = Object::instanceOf(self, floatBase);
    Object *intObj2 = Object::instanceOf(*arguments, intBase);
    Object *floatObj2 = Object::instanceOf(*arguments, floatBase);

    VM_ASSERT_TYPE(floatObj1, "Float");

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

static void floatToInt(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT_ARITY(0_z, count);

    auto *floatBase = state->m_shared->m_valueCache.m_floatBase;
    auto *floatObj1 = (FloatObject *)Object::instanceOf(self, floatBase);

    state->m_resultValue = Object::newInt(state, floatObj1->m_value);
}

static void floatToString(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT_ARITY(0_z, count);

    auto *floatBase = state->m_shared->m_valueCache.m_floatBase;
    auto *floatObj1 = (FloatObject *)Object::instanceOf(self, floatBase);

    char format[1024];
    snprintf(format, sizeof format, "%g", floatObj1->m_value);
    if (!strchr(format, '.'))
        snprintf(format, sizeof format, "%g.0", floatObj1->m_value);

    state->m_resultValue = Object::newString(state, format, strlen(format));
}

/// [String]
static void stringAdd(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);
    VM_ASSERT(arguments[0], "cannot perform string concatenation with Null");

    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    Object *strObj1 = Object::instanceOf(self, stringBase);
    Object *strObj2 = Object::instanceOf(*arguments, stringBase);

    VM_ASSERT_TYPE(strObj1 && strObj2, "String");

    auto fmt = u::format("%s%s", ((StringObject *)strObj1)->m_value,
                                 ((StringObject *)strObj2)->m_value);

    state->m_resultValue = Object::newString(state, &fmt[0], fmt.size());
}

static void stringCompare(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);

    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    Object *strObj1 = Object::instanceOf(self, stringBase);
    Object *strObj2 = Object::instanceOf(*arguments, stringBase);

    VM_ASSERT_TYPE(strObj1 && strObj2, "String");

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
    VM_ASSERT_ARITY(1_z, count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    VM_ASSERT_TYPE(arrayObject, "Array");
    VM_ASSERT_TYPE(intObject, "Int");

    int oldSize = arrayObject->m_length;
    int newSize = intObject->m_value;

    VM_ASSERT(newSize >= 0, "'Array.resize(%d)' not allowed", newSize);

    arrayObject->m_contents = (Object **)Memory::reallocate(arrayObject->m_contents, sizeof(Object *) * newSize);
    memset(arrayObject->m_contents + oldSize, 0, sizeof(Object *) * (newSize - oldSize));
    arrayObject->m_length = newSize;

    Object::setNormal(self, "length", Object::newInt(state, newSize));

    state->m_resultValue = self;
}

static void arrayPush(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);

    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    VM_ASSERT_TYPE(arrayObject, "Array");

    Object *value = *arguments;
    arrayObject->m_contents = (Object **)Memory::reallocate(arrayObject->m_contents, sizeof(Object *) * ++arrayObject->m_length);
    arrayObject->m_contents[arrayObject->m_length - 1] = value;

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_resultValue = self;
}

static void arrayPop(State *state, Object *self, Object *, Object **, size_t count) {
    VM_ASSERT_ARITY(0_z, count);

    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);

    VM_ASSERT_TYPE(arrayObject, "Array");

    Object *result = arrayObject->m_contents[arrayObject->m_length - 1];
    arrayObject->m_contents = (Object **)Memory::reallocate(arrayObject->m_contents, sizeof(Object *) * --arrayObject->m_length);

    Object::setNormal(self, "length", Object::newInt(state, arrayObject->m_length));

    state->m_resultValue = result;
}

static void arrayIndex(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    if (intObject) {
        VM_ASSERT_TYPE(arrayObject, "Array");
        const int index = intObject->m_value;
        VM_ASSERT(index >= 0 && index < arrayObject->m_length, "index out of range");
        state->m_resultValue = arrayObject->m_contents[index];
    } else {
        state->m_resultValue = nullptr;
    }
}

static void arrayIndexAssign(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(2_z, count);

    Object *intBase = state->m_shared->m_valueCache.m_intBase;
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;

    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *intObject = (IntObject *)Object::instanceOf(*arguments, intBase);

    VM_ASSERT_TYPE(arrayObject, "Array");
    VM_ASSERT_TYPE(intObject, "Int");

    const int index = intObject->m_value;
    VM_ASSERT(index >= 0 && index < arrayObject->m_length, "index out of range");
    arrayObject->m_contents[index] = arguments[1];
    state->m_resultValue = nullptr;
}

static void arrayJoin(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);

    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;
    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    auto *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    auto *stringObject = (StringObject *)Object::instanceOf(*arguments, stringBase);

    VM_ASSERT_TYPE(arrayObject, "Array");
    VM_ASSERT_TYPE(stringObject, "String");

    const size_t length = strlen(stringObject->m_value);
    size_t resultLength = 0;
    for (int i = 0; i < arrayObject->m_length; i++) {
        if (i > 0) resultLength += length;
        auto *entryObject = (StringObject *)Object::instanceOf(arrayObject->m_contents[i], stringBase);
        VM_ASSERT(entryObject, "Array.join() with '[%d]' non-string not allowed", i);
        resultLength += strlen(entryObject->m_value);
    }

    char *result = (char *)Memory::allocate(resultLength + 1);
    char *current = result;
    for (int i = 0; i < arrayObject->m_length; i++) {
        if (i > 0) {
            memcpy(current, stringObject->m_value, length);
            current += length;
        }
        auto *entryObject = (StringObject *)Object::instanceOf(arrayObject->m_contents[i], stringBase);
        const size_t entryLength = strlen(entryObject->m_value);
        memcpy(current, entryObject->m_value, entryLength);
        current += entryLength;
    }
    current[0] = '\0';

    state->m_resultValue = Object::newString(state, result, resultLength);
    Memory::free(result);
}

enum { kSin, kCos, kTan, kSqrt };

static void mathTrig(State *state, Object *, Object **arguments, size_t count, int type) {
    VM_ASSERT_ARITY(1_z, count);

    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;
    FloatObject *floatObject = (FloatObject *)Object::instanceOf(*arguments, floatBase);

    VM_ASSERT_TYPE(floatObject, "Float");

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
    VM_ASSERT_ARITY(2_z, count);

    Object *floatBase = state->m_shared->m_valueCache.m_floatBase;

    FloatObject *lhsObject = (FloatObject *)Object::instanceOf(arguments[0], floatBase);
    FloatObject *rhsObject = (FloatObject *)Object::instanceOf(arguments[1], floatBase);

    VM_ASSERT_TYPE(lhsObject && rhsObject, "Float");

    state->m_resultValue = Object::newFloat(state, m::pow(lhsObject->m_value, rhsObject->m_value));
}

// [Function]
static void functionApply(State *state, Object *self, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);
    Object *arrayBase = state->m_shared->m_valueCache.m_arrayBase;
    auto *arrayObject = (ArrayObject *)Object::instanceOf(*arguments, arrayBase);
    VM_ASSERT_TYPE(arrayObject, "Array");
    VM::callCallable(state, nullptr, self, arrayObject->m_contents, arrayObject->m_length);
}

// [Root]
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

static void require(State *state, Object *, Object *, Object **arguments, size_t count) {
    VM_ASSERT_ARITY(1_z, count);

    Object *root = state->m_root;
    Object *stringBase = state->m_shared->m_valueCache.m_stringBase;

    auto *stringObject = (StringObject *)Object::instanceOf(arguments[0], stringBase);
    VM_ASSERT_TYPE(stringObject, "parameter to 'require()' must be string");

    u::string fileName = stringObject->m_value;
    SourceRange source = SourceRange::readFile(&fileName[0], false);
    if (!source.m_begin) {
        // Try the game path as an alternative
        fileName = neoGamePath() + stringObject->m_value;
        source = SourceRange::readFile(&fileName[0]);
    }
    if (!source.m_begin)
        return;

    // duplicate the filename since the stringObject can go out of scope
    const size_t length = fileName.size() + 1;
    char *copy = (char *)Memory::allocate(length);
    memcpy(copy, &fileName[0], length);
    SourceRecord::registerSource(source, copy, 0, 0);

    UserFunction *module = nullptr;
    char *text = source.m_begin;
    ParseResult result = Parser::parseModule(&text, &module);
    VM_ASSERT(result == kParseOk, "parsing failed in 'require()'");

    // dump it
    //UserFunction::dump(module, 0);

    State subState = { };
    subState.m_parent = state;
    subState.m_root = state->m_root;
    subState.m_shared = state->m_shared;

    VM::callFunction(&subState, root, module, nullptr, 0);
    VM::run(&subState);

    if (subState.m_runState == kErrored) {
        state->m_runState = kErrored;
        state->m_error = "require failed";
        return;
    }

    state->m_resultValue = subState.m_resultValue;
}

const char *getTypeString(State *state, Object *object) {
    if (object) {
        if (object == state->m_shared->m_valueCache.m_intBase)
            return "Int";
        if (object == state->m_shared->m_valueCache.m_boolBase)
            return "Bool";
        if (object == state->m_shared->m_valueCache.m_floatBase)
            return "Float";
        if (object == state->m_shared->m_valueCache.m_closureBase)
            return "Closure";
        if (object == state->m_shared->m_valueCache.m_functionBase)
            return "Function";
        if (object == state->m_shared->m_valueCache.m_arrayBase)
            return "Array";
        if (object == state->m_shared->m_valueCache.m_stringBase)
            return "String";
        if (object->m_parent)
            return getTypeString(state, object->m_parent);
        U_ASSERT(false && "unimplemented");
    }
    return "Null";
}

Object *createRoot(State *state) {
    Object *root = Object::newObject(state, nullptr);

    state->m_root = root;

    RootSet pinned;
    GC::addRoots(state, &root, 1, &pinned);

    // null
    Object::setNormal(root, "Null", nullptr);

    // function
    Object *functionObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_functionBase = functionObject;
    functionObject->m_flags |= kNoInherit;
    Object::setNormal(root, "Function", functionObject);
    Object::setNormal(functionObject, "apply", Object::newFunction(state, functionApply));
    functionObject->m_flags |= kImmutable;

    // closure
    Object *closureObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_closureBase = closureObject;
    closureObject->m_flags |= kNoInherit;
    Object::setNormal(root, "Closure", closureObject);
    Object::setNormal(closureObject, "apply", Object::newFunction(state, functionApply));
    closureObject->m_mark = closureMark;

    // bool
    Object *boolObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_boolBase = boolObject;
    boolObject->m_flags |= kNoInherit;
    Object::setNormal(root, "Bool", boolObject);
    Object::setNormal(boolObject, "!", Object::newFunction(state, boolNot));
    Object::setNormal(boolObject, "==", Object::newFunction(state, boolCmp));
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
    Object::setNormal(root, "Int", intObject);
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
    Object::setNormal(intObject, "toFloat", Object::newFunction(state, intToFloat));
    Object::setNormal(intObject, "toString", Object::newFunction(state, intToString));
    state->m_shared->m_valueCache.m_intZero = Object::newInt(state, 0);
    GC::addPermanent(state, state->m_shared->m_valueCache.m_intZero);
    intObject->m_flags |= kImmutable;

    // float
    Object *floatObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_floatBase = floatObject;
    floatObject->m_flags |= kNoInherit;
    Object::setNormal(root, "Float", floatObject);
    Object::setNormal(floatObject, "+", Object::newFunction(state, floatAdd));
    Object::setNormal(floatObject, "-", Object::newFunction(state, floatSub));
    Object::setNormal(floatObject, "*", Object::newFunction(state, floatMul));
    Object::setNormal(floatObject, "/", Object::newFunction(state, floatDiv));
    Object::setNormal(floatObject, "==", Object::newFunction(state, floatCompareEq));
    Object::setNormal(floatObject, "<", Object::newFunction(state, floatCompareLt));
    Object::setNormal(floatObject, ">", Object::newFunction(state, floatCompareGt));
    Object::setNormal(floatObject, "<=", Object::newFunction(state, floatCompareLe));
    Object::setNormal(floatObject, ">=", Object::newFunction(state, floatCompareGe));
    Object::setNormal(floatObject, "toInt", Object::newFunction(state, floatToInt));
    Object::setNormal(floatObject, "toString", Object::newFunction(state, floatToString));
    floatObject->m_flags |= kImmutable;

    // string
    Object *stringObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_stringBase = stringObject;
    stringObject->m_flags |= kNoInherit;
    Object::setNormal(root, "String", stringObject);
    Object::setNormal(stringObject, "+", Object::newFunction(state, stringAdd));
    Object::setNormal(stringObject, "==", Object::newFunction(state, stringCompare));
    stringObject->m_flags |= kImmutable;

    // array
    Object *arrayObject = Object::newObject(state, nullptr);
    state->m_shared->m_valueCache.m_arrayBase = arrayObject;
    arrayObject->m_flags |= kNoInherit;
    Object::setNormal(root, "Array", arrayObject);
    arrayObject->m_mark = arrayMark;
    Object::setNormal(arrayObject, "resize", Object::newFunction(state, arrayResize));
    Object::setNormal(arrayObject, "join", Object::newFunction(state, arrayJoin));
    Object::setNormal(arrayObject, "push", Object::newFunction(state, arrayPush));
    Object::setNormal(arrayObject, "pop", Object::newFunction(state, arrayPop));
    Object::setNormal(arrayObject, "[]", Object::newFunction(state, arrayIndex));
    Object::setNormal(arrayObject, "[]=", Object::newFunction(state, arrayIndexAssign));
    arrayObject->m_flags |= kClosed | kImmutable;

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

    // others
    Object::setNormal(root, "print", Object::newFunction(state, print));
    Object::setNormal(root, "require", Object::newFunction(state, require));

    GC::delRoots(state, &pinned);

    return root;
}

}
