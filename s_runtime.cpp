#include <string.h>

#include "s_object.h"
#include "s_instr.h"
#include "s_gc.h"

#include "u_assert.h"
#include "u_new.h"
#include "u_log.h"

#include "engine.h" // neoFatal

namespace s {

static Object *equals(Object *context, Object *self, Object *function, Object **args, size_t length) {
    (void)function;
    (void)self;

    U_ASSERT(length == 2);

    Object *root = context;
    while (root->m_parent) root = root->m_parent;
    Object *intBase = root->m_table.lookup("int");
    Object *floatBase = root->m_table.lookup("float");

    Object *object1 = args[0];
    Object *object2 = args[1];
    if (object1->m_parent == intBase && object2->m_parent == intBase) {
        bool test = ((IntObject *)object1)->m_value == ((IntObject *)object2)->m_value;
        return Object::newBoolean(context, test);
    }

    if ((object1->m_parent == floatBase || object1->m_parent == intBase) &&
        (object2->m_parent == floatBase || object2->m_parent == intBase))
    {
        float value1 = object1->m_parent == floatBase ? ((FloatObject *)object1)->m_value : ((IntObject *)object1)->m_value;
        float value2 = object2->m_parent == floatBase ? ((FloatObject *)object2)->m_value : ((IntObject *)object2)->m_value;
        return Object::newBoolean(context, value1 == value2);
    }

    U_UNREACHABLE();
}

#define MATHOP(NAME, OP) \
static Object *NAME(Object *context, Object *self, Object *function, Object **args, size_t length) { \
    (void)function; \
    (void)self; \
    U_ASSERT(length == 2); \
    Object *root = context; \
    while (root->m_parent) \
        root = root->m_parent; \
    Object *intBase = root->m_table.lookup("int"); \
    Object *floatBase = root->m_table.lookup("float"); \
    Object *object1 = args[0]; \
    Object *object2 = args[1]; \
    if (object1->m_parent == intBase && object2->m_parent == intBase) { \
        int result = ((IntObject *)object1)->m_value OP ((IntObject *)object2)->m_value; \
        return Object::newInt(context, result); \
    } \
    if ((object1->m_parent == floatBase || object1->m_parent == intBase) && \
        (object2->m_parent == floatBase || object2->m_parent == intBase)) \
    { \
        float value1 = object1->m_parent == floatBase ? ((FloatObject *)object1)->m_value : ((IntObject *)object1)->m_value; \
        float value2 = object2->m_parent == floatBase ? ((FloatObject *)object2)->m_value : ((IntObject *)object2)->m_value; \
        return Object::newFloat(context, value1 OP value2); \
    } \
    U_UNREACHABLE(); \
}

MATHOP(add, +)
MATHOP(sub, -)
MATHOP(mul, *)
MATHOP(div, /)

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
    Object *intBase = root->m_table.lookup("int");
    Object *floatBase = root->m_table.lookup("float");
    Object *stringBase = root->m_table.lookup("string");

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
    }
    u::Log::out("\n");
    return nullptr;
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
        case Instr::kGetRoot: {
            auto *getRoot = (GetRootInstr *)instr;
            Slot slot = getRoot->m_slot;
            Object *root = context;
            while (root->m_parent) root = root->m_parent;
            U_ASSERT(slot < numSlots && !slots[slot]);
            slots[getRoot->m_slot] = root;
        } break;

        case Instr::kGetContext: {
            auto *getContext = (GetContextInstr *)instr;
            Slot slot = getContext->m_slot;
            U_ASSERT(slot < numSlots && !slots[slot]);
            slots[slot] = context;
        } break;

        case Instr::kAccess: {
            auto *access = (AccessInstr *)instr;
            Slot targetSlot = access->m_targetSlot;
            Slot objectSlot = access->m_objectSlot;
            const char *key = access->m_key;
            U_ASSERT(targetSlot < numSlots && !slots[targetSlot]);
            U_ASSERT(objectSlot < numSlots);
            for (Object *object = slots[objectSlot]; object; object = object->m_parent) {
                Object *value = object->m_table.lookup(key);
                if (value) {
                    slots[targetSlot] = value;
                    break;
                }
            }
        } break;

        case Instr::kAssign: {
            AssignInstr *i = (AssignInstr *)instr;
            Slot objectSlot = i->m_objectSlot;
            Slot valueSlot = i->m_valueSlot;
            const char *key = i->m_key;
            U_ASSERT(objectSlot < numSlots);
            U_ASSERT(valueSlot < numSlots);
            Object *object = slots[objectSlot];
            object->set(key, slots[valueSlot]);
        } break;

        case Instr::kAssignExisting: {
            AssignExistingInstr *i = (AssignExistingInstr *)instr;
            Slot objectSlot = i->m_objectSlot;
            Slot valueSlot = i->m_valueSlot;
            const char *key = i->m_key;
            U_ASSERT(objectSlot < numSlots);
            U_ASSERT(valueSlot < numSlots);
            Object *object = slots[objectSlot];
            object->setExisting(key, slots[valueSlot]);
        } break;

        case Instr::kAllocObject: {
            AllocObjectInstr *i = (AllocObjectInstr *)instr;
            Slot targetSlot = i->m_targetSlot;
            Slot parentSlot = i->m_parentSlot;
            U_ASSERT(targetSlot < numSlots && !slots[targetSlot]);
            U_ASSERT(parentSlot < numSlots);
            slots[targetSlot] = Object::newObject(slots[parentSlot]);
        } break;

        case Instr::kAllocClosureObject: {
            AllocClosureObjectInstr *i = (AllocClosureObjectInstr *)instr;
            Slot targetSlot = i->m_targetSlot;
            Slot contextSlot = i->m_contextSlot;
            U_ASSERT(targetSlot < numSlots && !slots[targetSlot]);
            U_ASSERT(contextSlot < numSlots);
            slots[targetSlot] = Object::newClosure(slots[contextSlot], i->m_function);
        } break;

        case Instr::kAllocIntObject: {
            AllocIntObjectInstr *i = (AllocIntObjectInstr *)instr;
            Slot target = i->m_targetSlot;
            int value = i->m_value;
            U_ASSERT(target < numSlots && !slots[target]);
            slots[target] = Object::newInt(context, value);
        } break;

        case Instr::kAllocFloatObject: {
            AllocFloatObjectInstr *i = (AllocFloatObjectInstr *)instr;
            Slot target = i->m_targetSlot;
            float value = i->m_value;
            U_ASSERT(target < numSlots && !slots[target]);
            slots[target] = Object::newFloat(context, value);
        } break;

        case Instr::kAllocStringObject: {
            AllocStringObjectInstr *i = (AllocStringObjectInstr *)instr;
            Slot target = i->m_targetSlot;
            const char *value = i->m_value;
            U_ASSERT(target < numSlots && !slots[target]);
            slots[target] = Object::newString(context, value);
        } break;

        case Instr::kCloseObject: {
            CloseObjectInstr *i = (CloseObjectInstr *)instr;
            Slot slot = i->m_slot;
            U_ASSERT(slot < numSlots);
            Object *object = slots[slot];
            U_ASSERT(object);
            U_ASSERT(!(object->m_flags & Object::kClosed));
            object->m_flags |= Object::kClosed;
        } break;

        case Instr::kCall: {
            auto *call = (CallInstr *)instr;
            Slot targetSlot = call->m_targetSlot;
            Slot functionSlot = call->m_functionSlot;
            Slot argsLength = call->m_length;
            U_ASSERT(targetSlot < numSlots && !slots[targetSlot]);
            U_ASSERT(functionSlot < numSlots);
            Object *functionObject = slots[functionSlot];
            Object *root = context;
            while (root->m_parent) root = root->m_parent;
            // validate function type
            Object *functionBase = root->m_table.lookup("function");
            Object *closureBase = root->m_table.lookup("closure");
            Object *functionType = functionObject->m_parent;
            while (functionType->m_parent)
                functionType = functionType->m_parent;
            U_ASSERT(functionType == functionBase || functionType == closureBase);
            FunctionObject *function = (FunctionObject *)functionObject;
            // form the argument array from slots
            Object **args = (Object **)neoMalloc(sizeof *args * argsLength);
            for (size_t i = 0; i < argsLength; i++) {
                Slot argSlot = call->m_arguments[i];
                U_ASSERT(argSlot < numSlots);
                args[i] = slots[argSlot];
            }
            // now call
            slots[targetSlot] = function->m_function(context, nullptr, functionObject, args, argsLength);
            neoFree(args);
        } break;

        case Instr::kReturn: {
            auto *ret = (ReturnInstr *)instr;
            Slot returnSlot = ret->m_returnSlot;
            U_ASSERT(returnSlot < numSlots);
            Object *result = slots[returnSlot];
            GC::removeRoots(frameRoots);
            return result;
        } break;

        case Instr::kBranch: {
            auto *branch = (BranchInstr *)instr;
            size_t blockIndex = branch->m_block;
            U_ASSERT(blockIndex < function->m_body.m_length);
            block = &function->m_body.m_blocks[blockIndex];
            offset = 0;
        } break;

        case Instr::kTestBranch: {
            auto *testBranch = (TestBranchInstr *)instr;
            Slot testSlot = testBranch->m_testSlot;
            size_t trueBlock = testBranch->m_trueBlock;
            size_t falseBlock = testBranch->m_falseBlock;
            U_ASSERT(testSlot < numSlots);
            Object *testValue = slots[testSlot];

            Object *root = context;
            while (root->m_parent) root = root->m_parent;
            Object *booleanBase = root->m_table.lookup("boolean");
            Object *intBase = root->m_table.lookup("int");

            bool test = false;
            if (testValue && testValue->m_parent == booleanBase) {
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
    context->set("self", self);
    return callFunction(context, &closureObject->m_userFunction, args, length);
}

Object *createRoot() {
    Object *root = Object::newObject(nullptr);
    Object *functionObject = Object::newObject(nullptr);

    UserFunction magic;
    Object *closureObject = Object::newClosure(root, &magic);
    ((ClosureObject *)closureObject)->m_context = nullptr;
    ((ClosureObject *)closureObject)->m_function = nullptr;

    closureObject->set("mark", Object::newFunction(root, mark));

    root->set("int", Object::newObject(nullptr));
    root->set("boolean", Object::newObject(nullptr));
    root->set("float", Object::newObject(nullptr));
    root->set("string", Object::newObject(nullptr));

    root->set("function", functionObject);
    root->set("closure", closureObject);

    root->set("=", Object::newFunction(root, equals));
    root->set("+", Object::newFunction(root, add));
    root->set("-", Object::newFunction(root, sub));
    root->set("*", Object::newFunction(root, mul));
    root->set("/", Object::newFunction(root, div));

    root->set("print", Object::newFunction(root, print));
    return root;
}

}
