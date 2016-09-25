#include <string.h>

#include "s_object.h"
#include "s_instr.h"
#include "s_gc.h"

#include "u_assert.h"
#include "u_new.h"
#include "u_log.h"

#include "engine.h" // neoFatal

namespace s {

static void vmGetRoot(Object *context, GetRootInstr *instruction, Object **slots, size_t numSlots) {
    Slot slot = instruction->m_slot;
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    U_ASSERT(slot < numSlots);
    slots[slot] = root;
}

static void vmGetContext(Object *context, GetContextInstr *instruction, Object **slots, size_t numSlots) {
    Slot slot = instruction->m_slot;
    U_ASSERT(slot < numSlots);
    slots[slot] = context;
}

static void vmAccess(Object *context, AccessInstr *instruction, Object **slots, size_t numSlots) {
    Slot targetSlot = instruction->m_targetSlot;
    Slot objectSlot = instruction->m_objectSlot;
    Slot keySlot = instruction->m_keySlot;

    U_ASSERT(keySlot < numSlots && slots[keySlot]);

    const char *key = ((StringObject *)slots[keySlot])->m_value;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(objectSlot < numSlots);

    Object *object = slots[objectSlot];

    bool found;
    Object *value = object->lookup(key, &found);
    if (!found) {
        U_ASSERT(0 && "identifier not found");
    }

    slots[targetSlot] = value;
}

static void vmAssignNormal(Object *context, AssignNormalInstr *instruction, Object **slots, size_t numSlots) {
    Slot objectSlot = instruction->m_objectSlot;
    Slot valueSlot = instruction->m_valueSlot;
    Slot keySlot = instruction->m_keySlot;

    U_ASSERT(keySlot < numSlots && slots[keySlot]);

    const char *key = ((StringObject *)slots[keySlot])->m_value;

    U_ASSERT(objectSlot < numSlots);
    U_ASSERT(valueSlot < numSlots);

    Object *object = slots[objectSlot];
    object->setNormal(key, slots[valueSlot]);
}

static void vmAssignExisting(Object *context, AssignExistingInstr *instruction, Object **slots, size_t numSlots) {
    Slot objectSlot = instruction->m_objectSlot;
    Slot valueSlot = instruction->m_valueSlot;
    Slot keySlot = instruction->m_keySlot;

    U_ASSERT(keySlot < numSlots && slots[keySlot]);

    const char *key = ((StringObject *)slots[keySlot])->m_value;

    U_ASSERT(objectSlot < numSlots);
    U_ASSERT(valueSlot < numSlots);

    Object *object = slots[objectSlot];
    object->setExisting(key, slots[valueSlot]);
}

static void vmAssignShadowing(Object *context, AssignShadowingInstr *instruction, Object **slots, size_t numSlots) {
    Slot objectSlot = instruction->m_objectSlot;
    Slot valueSlot = instruction->m_valueSlot;
    Slot keySlot = instruction->m_keySlot;

    U_ASSERT(keySlot < numSlots && slots[keySlot]);

    const char *key = ((StringObject *)slots[keySlot])->m_value;

    U_ASSERT(objectSlot < numSlots);
    U_ASSERT(valueSlot < numSlots);

    Object *object = slots[objectSlot];
    object->setShadowing(key, slots[valueSlot]);
}

static void vmAllocateObject(Object *context, AllocObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot targetSlot = instruction->m_targetSlot;
    Slot parentSlot = instruction->m_parentSlot;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(parentSlot < numSlots);

    slots[targetSlot] = Object::newObject(slots[parentSlot]);
}

static void vmAllocateClosureObject(Object *context, AllocClosureObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot targetSlot = instruction->m_targetSlot;
    Slot contextSlot = instruction->m_contextSlot;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(contextSlot < numSlots);

    slots[targetSlot] = Object::newClosure(slots[contextSlot], instruction->m_function);
}

static void vmAllocateIntObject(Object *context, AllocIntObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot target = instruction->m_targetSlot;
    int value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newInt(context, value);
}

static void vmAllocateFloatObject(Object *context, AllocFloatObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot target = instruction->m_targetSlot;
    float value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newFloat(context, value);
}

static void vmAllocateStringObject(Object *context, AllocStringObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot target = instruction->m_targetSlot;
    const char *value = instruction->m_value;

    U_ASSERT(target < numSlots);

    slots[target] = Object::newString(context, value);
}

static void vmCloseObject(Object *context, CloseObjectInstr *instruction, Object **slots, size_t numSlots) {
    Slot slot = instruction->m_slot;
    U_ASSERT(slot < numSlots);
    Object *object = slots[slot];
    U_ASSERT(!(object->m_flags & Object::kClosed));
    object->m_flags |= Object::kClosed;
}

// Marshalls all argument data into slots of the functions locals
static void vmCall(Object *context, CallInstr *instruction, Object **slots, size_t numSlots) {
    Slot targetSlot = instruction->m_targetSlot;
    Slot functionSlot = instruction->m_functionSlot;
    Slot thisSlot = instruction->m_thisSlot;
    Slot argsLength = instruction->m_length;

    U_ASSERT(targetSlot < numSlots);
    U_ASSERT(functionSlot < numSlots);
    U_ASSERT(thisSlot < numSlots);

    Object *thisObject = slots[thisSlot];
    Object *functionObject = slots[functionSlot];
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;

    // validate function type
    Object *functionBase = root->lookup("function", nullptr);
    Object *closureBase = root->lookup("closure", nullptr);
    Object *functionType = functionObject->m_parent;
    while (functionType->m_parent)
        functionType = functionType->m_parent;
    U_ASSERT(functionType == functionBase || functionType == closureBase);

    FunctionObject *function = (FunctionObject *)functionObject;

    // form the argument array from slots
    Object **args = (Object **)neoMalloc(sizeof *args * argsLength);
    for (size_t i = 0; i < argsLength; i++) {
        Slot argSlot = instruction->m_arguments[i];
        U_ASSERT(argSlot < numSlots);
        args[i] = slots[argSlot];
    }

    slots[targetSlot] = function->m_function(context, thisObject, functionObject, args, argsLength);
    neoFree(args);
}

Object *callFunction(Object *context, UserFunction *function, Object **args, size_t length) {
    // allocate slots for arguments and locals
    size_t numSlots = function->m_arity + function->m_slots;
    Object **slots = allocate<Object *>(numSlots);
    void *frameRoots = GC::addRoots(slots, numSlots);

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
            vmGetRoot(context, (GetRootInstr *)instr, slots, numSlots);
            break;
        case Instr::kGetContext:
            vmGetContext(context, (GetContextInstr *)instr, slots, numSlots);
            break;
        case Instr::kAccess:
            vmAccess(context, (AccessInstr *)instr, slots, numSlots);
            break;
        case Instr::kAssignNormal:
            vmAssignNormal(context, (AssignNormalInstr *)instr, slots, numSlots);
            break;
        case Instr::kAssignExisting:
            vmAssignExisting(context, (AssignExistingInstr *)instr, slots, numSlots);
            break;
        case Instr::kAssignShadowing:
            vmAssignShadowing(context, (AssignShadowingInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocObject:
            vmAllocateObject(context, (AllocObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocClosureObject:
            vmAllocateClosureObject(context, (AllocClosureObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocIntObject:
            vmAllocateIntObject(context, (AllocIntObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocFloatObject:
            vmAllocateFloatObject(context, (AllocFloatObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kAllocStringObject:
            vmAllocateStringObject(context, (AllocStringObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kCloseObject:
            vmCloseObject(context, (CloseObjectInstr *)instr, slots, numSlots);
            break;
        case Instr::kCall:
            vmCall(context, (CallInstr *)instr, slots, numSlots);
            break;

        // The following instructions change the program counter
        case Instr::kReturn: {
            auto *ret = (ReturnInstr *)instr;
            Slot returnSlot = ret->m_returnSlot;
            U_ASSERT(returnSlot < numSlots);
            Object *result = slots[returnSlot];
            GC::removeRoots(frameRoots);
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

            Object *root = context;
            while (root->m_parent)
                root = root->m_parent;
            Object *boolBase = root->lookup("bool", nullptr);
            Object *intBase = root->lookup("int", nullptr);

            bool test = false;
            if (testValue && testValue->m_parent == boolBase) {
                if (((BooleanObject *)testValue)->m_value == true)
                    test = true;
            } else if (testValue && testValue->m_parent == intBase) {
                if (((IntObject *)testValue)->m_value != 0)
                    test = true;
            } else {
                test = !!testValue;
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
    Object *context = Object::newObject(closureObject->m_context);
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
    Object *obj2 = *arguments;

    Object *intBase = root->lookup("int", nullptr);
    if (obj2->m_parent == intBase) {
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
    if (obj2->m_parent == floatBase) {
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
    Object *obj2 = *arguments;

    Object *intBase = root->lookup("int", nullptr);
    Object *floatBase = root->lookup("float", nullptr);

    if (obj2->m_parent == floatBase || obj2->m_parent == intBase) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = 0.0f;
        if (obj2->m_parent == floatBase)
            value2 = ((FloatObject*)obj2)->m_value;
        else
            value2 = ((IntObject*)obj2)->m_value;
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
    Object *obj2 = *arguments;

    if (obj1->m_parent == stringBase && obj2->m_parent == stringBase) {
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
    Object *obj2 = *arguments;

    Object *intBase = root->lookup("int", nullptr);

    if (obj2->m_parent == intBase) {
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
    if (obj2->m_parent == floatBase) {
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
    Object *obj2 = *arguments;

    Object *intBase = root->lookup("int", nullptr);
    Object *floatBase = root->lookup("float", nullptr);

    if (obj2->m_parent == intBase || obj2->m_parent == floatBase) {
        float value1 = ((FloatObject *)obj1)->m_value;
        float value2 = 0.0f;
        if (obj2->m_parent == floatBase)
            value2 = ((FloatObject*)obj2)->m_value;
        else
            value2 = ((IntObject*)obj2)->m_value;
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

static Object *mark(Object *context, Object *self, Object *function, Object **args, size_t length) {
    (void)context;
    (void)function;
    (void)args;
    (void)length;
    ClosureObject *closureObject = (ClosureObject *)self;
    if (closureObject->m_context)
        closureObject->m_context->mark();
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
        if (argument->m_parent == intBase) {
            u::Log::out("%d", ((IntObject *)argument)->m_value);
            continue;
        }
        if (argument->m_parent == floatBase) {
            u::Log::out("%f", ((FloatObject *)argument)->m_value);
            continue;
        }
        if (argument->m_parent == stringBase) {
            u::Log::out("%s", ((StringObject *)argument)->m_value);
            continue;
        }
        if (argument->m_parent == boolBase) {
            u::Log::out("%s", ((BooleanObject *)argument)->m_value ? "true" : "false");
            continue;
        }
    }
    u::Log::out("\n");
    return nullptr;
}

Object *createRoot() {
    Object *root = Object::newObject(nullptr);

    // pin the root when creating the root so the garbage collector doesn't
    // delete our objects while initializing
    void *pinned = GC::addRoots(&root, 1);

    // the null value
    root->setNormal("null", nullptr);

    Object *functionObject = Object::newObject(nullptr);
    root->setNormal("function", functionObject);

    // "closure" is the prototype for all closures, which means it contains a mark
    // function for the garbage collector. This means the prototype has to be a
    // closure itself without a context or function
    UserFunction closure;
    Object *closureObject = Object::newClosure(root, &closure);
    ((ClosureObject *)closureObject)->m_context = nullptr;
    ((ClosureObject *)closureObject)->m_function = nullptr;
    root->setNormal("closure", closureObject);
    closureObject->setNormal("mark", Object::newFunction(root, mark));

    Object *boolObject = Object::newObject(nullptr);
    root->setNormal("bool", boolObject);
    boolObject->setNormal("!", Object::newFunction(root, bool_not));

    Object *intObject = Object::newObject(nullptr);
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

    Object *floatObject = Object::newObject(nullptr);
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

    Object *stringObject = Object::newObject(nullptr);
    root->setNormal("string", stringObject);
    stringObject->setNormal("+", Object::newFunction(root, string_add));

    root->setNormal("print", Object::newFunction(root, print));

    GC::removeRoots(pinned);

    return root;
}

}
