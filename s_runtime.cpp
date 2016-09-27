#include <string.h>

#include "s_object.h"
#include "s_instr.h"
#include "s_gc.h"

#include "u_assert.h"
#include "u_new.h"
#include "u_log.h"

#include "engine.h" // neoFatal

namespace s {

static void vmGetRoot(Object *root, Object *context, GetRootInstr *instruction, Object **slots, size_t numSlots) {
    (void)context;
    Slot slot = instruction->m_slot;
    U_ASSERT(slot < numSlots);
    slots[slot] = root;
}

static void vmGetContext(Object *root, Object *context, GetContextInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;
    Slot slot = instruction->m_slot;
    U_ASSERT(slot < numSlots);
    slots[slot] = context;
}

static void vmAccess(Object *root, Object *context, AccessInstr *instruction, Object **slots, size_t numSlots) {
    (void)context;
    Slot targetSlot = instruction->m_targetSlot;
    Slot objectSlot = instruction->m_objectSlot;

    U_ASSERT(objectSlot < numSlots);
    Object *object = slots[objectSlot];

    Slot keySlot = instruction->m_keySlot;
    U_ASSERT(keySlot < numSlots && slots[keySlot]);
    Object *stringBase = root->lookup("string", nullptr);
    Object *keyObject = slots[keySlot];
    U_ASSERT(keyObject);
    StringObject *stringKey = (StringObject *)Object::instanceOf(slots[keySlot], stringBase);
    bool objectFound = false;
    Object *value = nullptr;
    if (stringKey) {
        const char *key = stringKey->m_value;
        value = object->lookup(key, &objectFound);
    }
    if (!objectFound) {
        Object *index = object->lookup("[]", nullptr);
        if (index) {
            Object *functionBase = root->lookup("function", nullptr);
            Object *closureBase = root->lookup("closure", nullptr);
            FunctionObject *function = (FunctionObject *)Object::instanceOf(index, functionBase);
            ClosureObject *closure = (ClosureObject *)Object::instanceOf(index, closureBase);
            U_ASSERT(function || closure);
            if (function)
                value = function->m_function(context, object, index, &keyObject, 1);
            else
                value = closure->m_function(context, object, index, &keyObject, 1);
            if (value)
                objectFound = true;
        }
    }
    if (!objectFound) {
        U_ASSERT(0 && "property not found");
    }

    U_ASSERT(targetSlot < numSlots);
    slots[targetSlot] = value;
}

static void vmAssign(Object *root, Object *context, AssignInstr *instruction, Object **slots, size_t numSlots) {
    (void)context;

    Slot objectSlot = instruction->m_objectSlot;
    Slot valueSlot = instruction->m_valueSlot;
    Slot keySlot = instruction->m_keySlot;
    U_ASSERT(objectSlot < numSlots);
    U_ASSERT(valueSlot < numSlots);
    U_ASSERT(keySlot < numSlots && slots[keySlot]);

    Object *stringBase = root->lookup("string", nullptr);
    Object *keyObject = slots[keySlot];
    Object *object = slots[objectSlot];
    StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);

    if (!stringKey) {
        Object *index = object->lookup("[]=", nullptr);
        if (index) {
            Object *functionBase = root->lookup("function", nullptr);
            Object *closureBase = root->lookup("closure", nullptr);
            FunctionObject *function = (FunctionObject *)Object::instanceOf(index, functionBase);
            ClosureObject *closure = (ClosureObject *)Object::instanceOf(index, closureBase);
            U_ASSERT(function || closure);
            Object *arguments[] = { keyObject, slots[valueSlot] };
            if (function)
                function->m_function(context, object, index, arguments, 2);
            else
                closure->m_function(context, object, index, arguments, 2);
            return;
        }
        U_ASSERT(0 && "internal error");
    }

    const char *key = stringKey->m_value;

    switch (instruction->m_assignType) {
    case AssignType::kPlain:
        if (object)
            object->setNormal(key, slots[valueSlot]);
        else U_ASSERT(0 && "assignment to null object");
        break;
    case AssignType::kExisting:
        object->setExisting(key, slots[valueSlot]);
        break;
    case AssignType::kShadowing:
        object->setShadowing(key, slots[valueSlot]);
    }
}

static void vmAllocateObject(Object *root, Object *context, AllocObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)context;

    Slot targetSlot = instruction->m_targetSlot;
    Slot parentSlot = instruction->m_parentSlot;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(parentSlot < numSlots);

    Object *parentObject = slots[parentSlot];
    if (parentObject) {
        // TODO(daleweiler): diasslow inheriting
    }
    Object *object = Object::newObject(root, slots[parentSlot]);
    slots[targetSlot] = object;
}

static void vmAllocateIntObject(Object *root, Object *context, AllocIntObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;

    Slot target = instruction->m_targetSlot;
    int value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newInt(context, value);
}

static void vmAllocateFloatObject(Object *root, Object *context, AllocFloatObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;

    Slot target = instruction->m_targetSlot;
    float value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newFloat(context, value);
}

static void vmAllocateArrayObject(Object *root, Object *context, AllocArrayObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;

    Slot target = instruction->m_targetSlot;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newArray(context, nullptr, 0);
}

static void vmAllocateStringObject(Object *root, Object *context, AllocStringObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;

    Slot target = instruction->m_targetSlot;
    const char *value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newString(context, value);
}

static void vmAllocateClosureObject(Object *root, Object *context, AllocClosureObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;
    (void)context;

    Slot targetSlot = instruction->m_targetSlot;
    Slot contextSlot = instruction->m_contextSlot;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(contextSlot < numSlots);

    slots[targetSlot] = Object::newClosure(slots[contextSlot], instruction->m_function);
}

static void vmCloseObject(Object *root, Object *context, CloseObjectInstr *instruction, Object **slots, size_t numSlots) {
    (void)root;
    (void)context;

    Slot slot = instruction->m_slot;
    U_ASSERT(slot < numSlots);
    Object *object = slots[slot];
    U_ASSERT(!(object->m_flags & Object::kClosed));
    object->m_flags |= Object::kClosed;
}

// Marshalls all argument data into slots of the functions locals
static void vmCall(Object *root, Object *context, CallInstr *instruction, Object **slots, size_t numSlots) {
    Slot targetSlot = instruction->m_targetSlot;
    Slot functionSlot = instruction->m_functionSlot;
    Slot thisSlot = instruction->m_thisSlot;
    size_t length = instruction->m_length;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(functionSlot < numSlots);
    U_ASSERT(thisSlot < numSlots);

    Object *thisObject = slots[thisSlot];
    Object *functionObject = slots[functionSlot];

    // validate function type
    Object *closureBase = root->lookup("closure", nullptr);
    Object *functionBase = root->lookup("function", nullptr);
    FunctionObject *function = (FunctionObject *)Object::instanceOf(functionObject, functionBase);
    ClosureObject *closure = (ClosureObject *)Object::instanceOf(functionObject, closureBase);
    U_ASSERT(function || closure);

    // form argument array from slots
    u::vector<Object *> arguments(length);
    for (size_t i = 0; i < length; i++) {
        Slot argumentSlot = instruction->m_arguments[i];
        U_ASSERT(argumentSlot < numSlots);
        arguments[i] = slots[argumentSlot];
    }

    Object *object;
    if (closure)
        object = closure->m_function(context, thisObject, functionObject, &arguments[0], arguments.size());
    else
        object = function->m_function(context, thisObject, functionObject, &arguments[0], arguments.size());
    slots[targetSlot] = object;
}

Object *callFunction(Object *context, UserFunction *function, Object **args, size_t length) {
    // allocate slots for arguments and locals
    size_t numSlots = function->m_arity + function->m_slots;

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object **slots = (Object **)neoCalloc(sizeof *slots, numSlots);
    void *frameRoots = GarbageCollector::addRoots(slots, numSlots);

    // ensure this call is valid
    U_ASSERT(length == function->m_arity);

    // copy the arguments into the slots
    for (size_t i = 0; i < length; ++i)
        slots[i] = args[i];

    // ensure this function has code
    U_ASSERT(function->m_body.m_length);
    InstrBlock *block = &function->m_body.m_blocks[0];
    size_t offset = 0;
    for (;;) {
        if (!(offset < block->m_length))
            neoFatal("reached end of block without a branch");

        Instr *instr = block->m_instrs[offset++];

        switch (instr->m_type) {
        case Instr::kGetRoot:
            vmGetRoot(root, context, (GetRootInstr *)instr, slots, numSlots);
            break;
        case Instr::kGetContext:
            vmGetContext(root, context, (GetContextInstr *)instr, slots, numSlots);
            break;
        case Instr::kAccess:
            vmAccess(root, context, (AccessInstr *)instr, slots, numSlots);
            break;
        case Instr::kAssign:
            vmAssign(root, context, (AssignInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocObject:
            vmAllocateObject(root, context, (AllocObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocClosureObject:
            vmAllocateClosureObject(root, context, (AllocClosureObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocIntObject:
            vmAllocateIntObject(root, context, (AllocIntObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocFloatObject:
            vmAllocateFloatObject(root, context, (AllocFloatObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocArrayObject:
            vmAllocateArrayObject(root, context, (AllocArrayObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocStringObject:
            vmAllocateStringObject(root, context, (AllocStringObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kCloseObject:
            vmCloseObject(root, context, (CloseObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kCall:
            vmCall(root, context, (CallInstr *)instr, slots, numSlots);
            break;

        // The following instructions change the program counter
        case Instr::kReturn: {
            auto *ret = (ReturnInstr *)instr;
            Slot returnSlot = ret->m_returnSlot;
            U_ASSERT(returnSlot < numSlots);
            Object *result = slots[returnSlot];
            GarbageCollector::delRoots(frameRoots);
            return result;
        } break;

        // TODO(daleweiler): cleanup
        case Instr::kBranch: {
            auto *branch = (BranchInstr *)instr;
            size_t blockIndex = branch->m_block;
            U_ASSERT(blockIndex < function->m_body.m_length);
            block = &function->m_body.m_blocks[blockIndex];
            offset = 0;
        } break;

        // TODO(daleweiler): cleanup & optimize
        case Instr::kTestBranch: {
            auto *testBranch = (TestBranchInstr *)instr;
            Slot testSlot = testBranch->m_testSlot;
            size_t trueBlock = testBranch->m_trueBlock;
            size_t falseBlock = testBranch->m_falseBlock;
            U_ASSERT(testSlot < numSlots);
            Object *testValue = slots[testSlot];

            bool test = false;

            // try boolean first
            Object *boolBase = root->lookup("bool", nullptr);
            Object *boolValue = Object::instanceOf(testValue, boolBase);
            if (boolValue) {
                test = ((BooleanObject *)boolValue)->m_value;
            } else {
                // then integer
                Object *intBase = root->lookup("int", nullptr);
                Object *intValue = Object::instanceOf(testValue, intBase);
                if (intValue)
                    test = !!((IntObject *)intValue)->m_value;
            }

            size_t targetBlock = test ? trueBlock : falseBlock;
            U_ASSERT(targetBlock < function->m_body.m_length);
            block = &function->m_body.m_blocks[targetBlock];
            offset = 0;
        } break;

        }
    }
}

Object *functionHandler(Object *callingContext, Object *self, Object *function, Object **args, size_t length) {
    // disacard calling context and self (lexical scoping)
    (void)callingContext;
    (void)self;
    ClosureObject *closureObject = (ClosureObject *)function;
    return callFunction(closureObject->m_context, &closureObject->m_userFunction, args, length);
}

Object *methodHandler(Object *callingContext, Object *self, Object *function, Object **args, size_t length) {
    (void)callingContext;
    ClosureObject *closureObject = (ClosureObject *)function;
    Object *context = Object::newObject(closureObject->m_context, closureObject->m_context);
    context->setNormal("this", self);
    return callFunction(context, &closureObject->m_userFunction, args, length);
}

// builtin functions
static Object *bool_not(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    (void)arguments;
    U_ASSERT(length == 0);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    return Object::newBoolean(context, !((BooleanObject*)self)->m_value);
}

// arithmetic
enum { kAdd, kSub, kMul, kDiv };

// [Int]
static Object *int_math(Object *context, Object *self, Object *function, Object **arguments, size_t length, int op) {
    (void)function;

    U_ASSERT(length == 1);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *obj1 = self;
    Object *obj2 = nullptr;

    Object *intBase = root->lookup("int", nullptr);
    obj2 = Object::instanceOf(*arguments, intBase);
    if (obj2) {
        int value1 = ((IntObject *)obj1)->m_value;
        int value2 = ((IntObject *)obj2)->m_value;
        switch (op) {
        case kAdd: return Object::newInt(context, value1 + value2);
        case kSub: return Object::newInt(context, value1 - value2);
        case kMul: return Object::newInt(context, value1 * value2);
        case kDiv: return Object::newInt(context, value1 / value2);
        }
    }

    Object *floatBase = root->lookup("float", nullptr);
    obj2 = Object::instanceOf(*arguments, floatBase);
    if (obj2) {
        float value1 = ((IntObject *)obj1)->m_value;
        float value2 = ((FloatObject *)obj2)->m_value;
        switch (op) {
        case kAdd: return Object::newFloat(context, value1 + value2);
        case kSub: return Object::newFloat(context, value1 - value2);
        case kMul: return Object::newFloat(context, value1 * value2);
        case kDiv: return Object::newFloat(context, value1 / value2);
        }
    }

    U_UNREACHABLE();
}

static Object *int_add(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_math(context, self, function, arguments, length, kAdd);
}

static Object *int_sub(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_math(context, self, function, arguments, length, kSub);
}

static Object *int_mul(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_math(context, self, function, arguments, length, kMul);
}

static Object *int_div(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_math(context, self, function, arguments, length, kDiv);
}

// [Float]
static Object *float_math(Object *context, Object *self, Object *function, Object **arguments, size_t length, int op) {
    (void)function;

    U_ASSERT(length == 1);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *obj1 = self;
    Object *obj2 = nullptr;

    Object *floatBase = root->lookup("float", nullptr);
    obj2 = Object::instanceOf(*arguments, floatBase);

    if (obj2) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = ((FloatObject *)obj2)->m_value;
        switch (op) {
        case kAdd: return Object::newFloat(context, value1 + value2);
        case kSub: return Object::newFloat(context, value1 - value2);
        case kMul: return Object::newFloat(context, value1 * value2);
        case kDiv: return Object::newFloat(context, value1 / value2);
        }
    }

    Object *intBase = root->lookup("int", nullptr);
    obj2 = Object::instanceOf(*arguments, intBase);
    if (obj2) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = ((IntObject *)obj2)->m_value;
        switch (op) {
        case kAdd: return Object::newFloat(context, value1 + value2);
        case kSub: return Object::newFloat(context, value1 - value2);
        case kMul: return Object::newFloat(context, value1 * value2);
        case kDiv: return Object::newFloat(context, value1 / value2);
        }
    }

    U_UNREACHABLE();
}

static Object *float_add(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_math(context, self, function, arguments, length, kAdd);
}

static Object *float_sub(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_math(context, self, function, arguments, length, kSub);
}

static Object *float_mul(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_math(context, self, function, arguments, length, kMul);
}

static Object *float_div(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_math(context, self, function, arguments, length, kDiv);
}

// [String]
static Object *string_add(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;

    U_ASSERT(length == 1);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *stringBase = root->lookup("string", nullptr);

    Object *obj1 = self;
    Object *obj2 = Object::instanceOf(*arguments, stringBase);

    if (obj2) {
        const char *string1 = ((StringObject *)obj1)->m_value;
        const char *string2 = ((StringObject *)obj2)->m_value;
        return Object::newString(context, u::format("%s%s", string1, string2).c_str());
    }

    U_UNREACHABLE();
}

// comparisons
enum { kEq, kLt, kGt, kLe, kGe };

// [Int]
static Object *int_compare(Object *context, Object *self, Object *function, Object **arguments, size_t length, int cmp) {
    (void)function;

    U_ASSERT(length == 1);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *obj1 = self;
    Object *obj2 = nullptr;

    Object *intBase = root->lookup("int", nullptr);
    obj2 = Object::instanceOf(*arguments, intBase);
    if (obj2) {
        int value1 = ((IntObject *)obj1)->m_value;
        int value2 = ((IntObject *)obj2)->m_value;
        switch (cmp) {
        case kEq: return Object::newBoolean(context, value1 == value2);
        case kLt: return Object::newBoolean(context, value1 < value2);
        case kGt: return Object::newBoolean(context, value1 > value2);
        case kLe: return Object::newBoolean(context, value1 <= value2);
        case kGe: return Object::newBoolean(context, value1 >= value2);
        }
    }

    Object *floatBase = root->lookup("float", nullptr);
    obj2 = Object::instanceOf(*arguments, floatBase);
    if (obj2) {
        float value1 = ((IntObject *)obj1)->m_value;
        float value2 = ((FloatObject *)obj2)->m_value;
        switch (cmp) {
        case kEq: return Object::newBoolean(context, value1 == value2);
        case kLt: return Object::newBoolean(context, value1 < value2);
        case kGt: return Object::newBoolean(context, value1 > value2);
        case kLe: return Object::newBoolean(context, value1 <= value2);
        case kGe: return Object::newBoolean(context, value1 >= value2);
        }
    }

    U_UNREACHABLE();
}

static Object *int_eq(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_compare(context, self, function, arguments, length, kEq);
}

static Object *int_lt(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_compare(context, self, function, arguments, length, kLt);
}

static Object *int_gt(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_compare(context, self, function, arguments, length, kGt);
}

static Object *int_le(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_compare(context, self, function, arguments, length, kLe);
}

static Object *int_ge(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return int_compare(context, self, function, arguments, length, kGe);
}

// [Float]
static Object *float_compare(Object *context, Object *self, Object *function, Object **arguments, size_t length, int cmp) {
    (void)function;

    U_ASSERT(length == 1);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *obj1 = self;
    Object *obj2 = nullptr;

    Object *floatBase = root->lookup("float", nullptr);
    obj2 = Object::instanceOf(*arguments, floatBase);

    if (obj2) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = ((FloatObject *)obj2)->m_value;
        switch (cmp) {
        case kEq: return Object::newBoolean(context, value1 == value2);
        case kLt: return Object::newBoolean(context, value1 < value2);
        case kGt: return Object::newBoolean(context, value1 > value2);
        case kLe: return Object::newBoolean(context, value1 <= value2);
        case kGe: return Object::newBoolean(context, value1 >= value2);
        }
    }

    Object *intBase = root->lookup("float", nullptr);
    obj2 = Object::instanceOf(*arguments, intBase);

    if (obj2) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = ((IntObject *)obj2)->m_value;
        switch (cmp) {
        case kEq: return Object::newBoolean(context, value1 == value2);
        case kLt: return Object::newBoolean(context, value1 < value2);
        case kGt: return Object::newBoolean(context, value1 > value2);
        case kLe: return Object::newBoolean(context, value1 <= value2);
        case kGe: return Object::newBoolean(context, value1 >= value2);
        }
    }

    U_UNREACHABLE();
}

static Object *float_eq(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_compare(context, self, function, arguments, length, kEq);
}

static Object *float_lt(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_compare(context, self, function, arguments, length, kLt);
}

static Object *float_gt(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_compare(context, self, function, arguments, length, kGt);
}

static Object *float_le(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_compare(context, self, function, arguments, length, kLe);
}

static Object *float_ge(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    return float_compare(context, self, function, arguments, length, kGe);
}

// [Closure]
static Object *closure_mark(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    (void)arguments;

    U_ASSERT(length == 0);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = root->lookup("closure", nullptr);
    ClosureObject *closureObject = (ClosureObject *)Object::instanceOf(self, closureBase);
    if (closureObject)
        Object::mark(context, closureObject->m_context);
    return nullptr;
}

// [Array]
static Object *array_mark(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    (void)arguments;

    U_ASSERT(length == 0);

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    if (arrayObject) {
        for (int i = 0; i < arrayObject->m_length; i++)
            Object::mark(context, arrayObject->m_contents[i]);
    }
    return nullptr;
}

static Object *array_resize(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    U_ASSERT(length == 1);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->lookup("int", nullptr);
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *resizeArgument = (IntObject *)Object::instanceOf(arguments[0], intBase);
    U_ASSERT(arrayObject);
    U_ASSERT(resizeArgument);
    int newSize = resizeArgument->m_value;
    U_ASSERT(newSize >= 0);
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * newSize);
    arrayObject->m_length = newSize;
    self->setNormal("length", Object::newInt(context, newSize));
    return self;
}

static Object *array_push(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    U_ASSERT(length == 1);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    U_ASSERT(arrayObject);
    Object *value = arguments[0];
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * ++arrayObject->m_length);
    arrayObject->m_contents[arrayObject->m_length - 1] = value;
    self->setNormal("length", Object::newInt(context, arrayObject->m_length));
    return self;
}

static Object *array_pop(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    (void)arguments;
    U_ASSERT(length == 0);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    U_ASSERT(arrayObject);
    Object *result = arrayObject->m_contents[arrayObject->m_length - 1];
    arrayObject->m_contents = (Object **)neoRealloc(arrayObject->m_contents, sizeof(Object *) * --arrayObject->m_length);
    self->setNormal("length", Object::newInt(context, arrayObject->m_length));
    return result;
}

static Object *array_index(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    U_ASSERT(length == 1);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->lookup("int", nullptr);
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *indexArgument = (IntObject *)Object::instanceOf(arguments[0], intBase);
    if (!indexArgument)
        return nullptr;
    U_ASSERT(arrayObject);
    int index = indexArgument->m_value;
    U_ASSERT(index >= 0 && index < arrayObject->m_length);
    return arrayObject->m_contents[index];
}

static Object *array_index_assign(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)function;
    U_ASSERT(length == 2);
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *intBase = root->lookup("int", nullptr);
    Object *arrayBase = root->lookup("array", nullptr);
    ArrayObject *arrayObject = (ArrayObject *)Object::instanceOf(self, arrayBase);
    IntObject *indexArgument = (IntObject *)Object::instanceOf(arguments[0], intBase);
    U_ASSERT(arrayObject);
    U_ASSERT(indexArgument);
    int index = indexArgument->m_value;
    U_ASSERT(index >= 0 && index < arrayObject->m_length);
    Object *value = arguments[1];
    arrayObject->m_contents[index] = value;
    return nullptr;
}

static Object *print(Object *context, Object *self, Object *function, Object **arguments, size_t length) {
    (void)self;
    (void)function;

    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    Object *intBase = root->lookup("int", nullptr);
    Object *floatBase = root->lookup("float", nullptr);
    Object *stringBase = root->lookup("string", nullptr);
    Object *boolBase = root->lookup("bool", nullptr);

    for (size_t i = 0; i < length; i++) {
        Object *argument = arguments[i];
        Object *instance = nullptr;
        instance = Object::instanceOf(argument, intBase);
        if (instance) {
            u::Log::out("%d", ((IntObject *)instance)->m_value);
            continue;
        }
        instance = Object::instanceOf(argument, floatBase);
        if (instance) {
            u::Log::out("%f", ((FloatObject *)instance)->m_value);
            continue;
        }
        instance = Object::instanceOf(argument, stringBase);
        if (instance) {
            u::Log::out("%s", ((StringObject *)instance)->m_value);
            continue;
        }
        instance = Object::instanceOf(argument, boolBase);
        if (instance) {
            u::Log::out("%s", ((BooleanObject *)instance)->m_value ? "true" : "false");
            continue;
        }
    }
    u::Log::out("\n");
    return nullptr;
}

Object *createRoot() {
    Object *root = Object::newObject(nullptr, nullptr);

    void *pinned = GarbageCollector::addRoots(&root, 1);

    // the null value
    root->setNormal("null", nullptr);

    Object *functionObject = Object::newObject(root, nullptr);
    root->setNormal("function", functionObject);

    Object *closureObject = Object::newObject(root, nullptr);
    root->setNormal("closure", closureObject);
    closureObject->setNormal("mark", Object::newFunction(root, closure_mark));

    Object *boolObject = Object::newObject(root, nullptr);
    root->setNormal("bool", boolObject);
    boolObject->setNormal("!", Object::newFunction(root, bool_not));

    Object *intObject = Object::newObject(root, nullptr);
    intObject->m_flags &= ~Object::kClosed;
    root->setNormal("int", intObject);
    intObject->setNormal("+", Object::newFunction(root, int_add));
    intObject->setNormal("-", Object::newFunction(root, int_sub));
    intObject->setNormal("*", Object::newFunction(root, int_mul));
    intObject->setNormal("/", Object::newFunction(root, int_div));
    intObject->setNormal("==", Object::newFunction(root, int_eq));
    intObject->setNormal("<", Object::newFunction(root, int_lt));
    intObject->setNormal(">", Object::newFunction(root, int_gt));
    intObject->setNormal("<=", Object::newFunction(root, int_le));
    intObject->setNormal(">=", Object::newFunction(root, int_ge));

    Object *floatObject = Object::newObject(root, nullptr);
    floatObject->m_flags &= ~Object::kClosed;
    root->setNormal("float", floatObject);
    floatObject->setNormal("+", Object::newFunction(root, float_add));
    floatObject->setNormal("-", Object::newFunction(root, float_sub));
    floatObject->setNormal("*", Object::newFunction(root, float_mul));
    floatObject->setNormal("/", Object::newFunction(root, float_div));
    floatObject->setNormal("==", Object::newFunction(root, float_eq));
    floatObject->setNormal("<", Object::newFunction(root, float_lt));
    floatObject->setNormal(">", Object::newFunction(root, float_gt));
    floatObject->setNormal("<=", Object::newFunction(root, float_le));
    floatObject->setNormal(">=", Object::newFunction(root, float_ge));

    Object *stringObject = Object::newObject(root, nullptr);
    root->setNormal("string", stringObject);
    stringObject->setNormal("+", Object::newFunction(root, string_add));

    Object *arrayObject = Object::newObject(root, nullptr);
    root->setNormal("array", arrayObject);
    arrayObject->setNormal("mark", Object::newFunction(root, array_mark));
    arrayObject->setNormal("resize", Object::newFunction(root, array_resize));
    arrayObject->setNormal("push", Object::newFunction(root, array_push));
    arrayObject->setNormal("pop", Object::newFunction(root, array_pop));
    arrayObject->setNormal("[]", Object::newFunction(root, array_index));
    arrayObject->setNormal("[]=", Object::newFunction(root, array_index_assign));

    root->setNormal("print", Object::newFunction(root, print));

    GarbageCollector::delRoots(pinned);

    return root;
}

}
