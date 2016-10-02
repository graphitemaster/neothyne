#include "s_object.h"
#include "s_gc.h"
#include "s_vm.h"

#include "u_assert.h"

namespace s {

CallFrame *VM::addFrame(State *state) {
    void *stack = (void *)state->m_stack;
    if (stack) {
        size_t capacity = *((size_t *)stack - 1);
        size_t newSize = sizeof(CallFrame) * (state->m_length + 1);
        if (newSize > capacity) {
            size_t newCapacity = capacity * 2;
            if (newSize > newCapacity)
                newCapacity = newSize;
            void *old = stack;
            void *base = (void *)((CallFrame*)stack - 1);
            stack = neoMalloc(newCapacity + sizeof(CallFrame));
            stack = (void *)((CallFrame *)stack + 1);
            *((size_t *)stack - 1) = newCapacity;
            // tear down old gc roots
            for (size_t i = 0; i < state->m_length; i++)
                GC::delRoots(state, &state->m_stack[i].m_root);
            memcpy(stack, old, capacity);
            state->m_stack = (CallFrame *)stack;
            // add new gc roots
            for (size_t i = 0; i < state->m_length; i++) {
                CallFrame *frame = &state->m_stack[i];
                GC::addRoots(state, frame->m_slots, frame->m_count, &frame->m_root);
            }
            neoFree(base);
        }
    } else {
        U_ASSERT(state->m_length == 0);
        size_t initialCapacity = sizeof(CallFrame);
        stack = neoMalloc(initialCapacity + sizeof(CallFrame));
        stack = (void *)((CallFrame *)stack + 1);
        *((size_t *)stack - 1) = initialCapacity;
        state->m_stack = (CallFrame *)stack;
    }
    state->m_length = state->m_length + 1;
    return &state->m_stack[state->m_length - 1];
}

void VM::delFrame(State *state) {
    CallFrame *frame = &state->m_stack[state->m_length - 1];
    GC::delRoots(state, &frame->m_root);
    neoFree(frame->m_slots);
    state->m_length = state->m_length - 1;
}

#define I(NAME, TYPE) \
    Instruction::TYPE *NAME = (Instruction::TYPE *)instruction

#define BEGIN {
#define BREAK } break

void VM::step(State *state, Object *root, void **preallocatedArguments) {
    CallFrame *frame = &state->m_stack[state->m_length - 1];
    Instruction *instruction = frame->m_block->m_instructions[frame->m_offset];
    switch (instruction->m_type) {
    case kGetRoot: BEGIN
        I(i, GetRoot);
        U_ASSERT(i->m_slot < frame->m_count);
        frame->m_slots[i->m_slot] = root;
        BREAK;

    case kGetContext: BEGIN
        I(i, GetContext);
        U_ASSERT(i->m_slot < frame->m_count);
        frame->m_slots[i->m_slot] = frame->m_context;
        BREAK;

    case kAccess:
    case kAccesStringKey: BEGIN
        I(access, Access);
        I(accessStringKey, AccessStringKey);
        Slot objectSlot;
        if (instruction->m_type == kAccess)
            objectSlot = access->m_objectSlot;
        else
            objectSlot = accessStringKey->m_objectSlot;

        U_ASSERT(objectSlot < frame->m_count);

        Object *object = frame->m_slots[objectSlot];

        const char *key = nullptr;
        bool hasKey = false;
        if (instruction->m_type == kAccess) {
            Slot keySlot = access->m_keySlot;

            U_ASSERT(keySlot < frame->m_count && frame->m_slots[keySlot]);

            Object *stringBase = Object::lookup(root, "string", nullptr);
            Object *keyObject = frame->m_slots[keySlot];

            StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
            if (stringKey) {
                key = stringKey->m_value;
                hasKey = true;
            }
        } else {
            key = accessStringKey->m_key;
            hasKey = true;
        }

        bool objectFound = false;
        if (hasKey)
            state->m_result = Object::lookup(object, key, &objectFound);
        if (!objectFound) {
            Object *indexOperation = Object::lookup(object, "[]", nullptr);
            if (indexOperation) {
                Object *functionBase = Object::lookup(root, "function", nullptr);
                Object *closureBase = Object::lookup(root, "closure", nullptr);

                FunctionObject *functionIndexOperation = (FunctionObject *)Object::instanceOf(indexOperation, functionBase);
                ClosureObject *closureIndexOperation = (ClosureObject *)Object::instanceOf(indexOperation, closureBase);

                U_ASSERT(functionIndexOperation || closureIndexOperation);

                Object *keyObject;
                if (instruction->m_type == kAccess)
                    keyObject = frame->m_slots[access->m_keySlot];
                else
                    keyObject = Object::newString(state, accessStringKey->m_key);

                if (functionIndexOperation)
                    functionIndexOperation->m_function(state, object, indexOperation, &keyObject, 1);
                else if (closureIndexOperation)
                    closureIndexOperation->m_function(state, object, indexOperation, &keyObject, 1);

                objectFound = true;
            }
        }
        if (!objectFound) {
            if (hasKey) {
                // could not find property by key
            } else {
                // could not find property by value
            }
            U_ASSERT(0 && "could not find property");
        }
        BREAK;

    case kAssign:
    case kAssignStringKey: BEGIN
        I(assign, Instruction::Assign);
        I(assignStringKey, Instruction::AssignStringKey);

        AssignType assignType;
        Object *object;
        Object *valueObject;
        const char *key;
        bool hasKey = false;

        if (instruction->m_type == kAssign) {
            Slot objectSlot = assign->m_objectSlot;
            Slot valueSlot = assign->m_valueSlot;
            Slot keySlot = assign->m_keySlot;

            U_ASSERT(objectSlot < frame->m_count);
            U_ASSERT(valueSlot < frame->m_count);
            U_ASSERT(keySlot < frame->m_count && frame->m_slots[keySlot]);

            object = frame->m_slots[objectSlot];
            valueObject = frame->m_slots[valueSlot];

            Object *stringBase = Object::lookup(root, "string", nullptr);
            Object *keyObject = frame->m_slots[keySlot];
            StringObject *stringKey = (StringObject *)Object::instanceOf(keyObject, stringBase);
            if (stringKey) {
                key = stringKey->m_value;
                assignType = assign->m_assignType;
                hasKey = true;
            }
        } else {
            Slot objectSlot = assignStringKey->m_objectSlot;
            Slot valueSlot = assignStringKey->m_valueSlot;
            U_ASSERT(objectSlot < frame->m_count);
            U_ASSERT(valueSlot < frame->m_count);
            object = frame->m_slots[objectSlot];
            valueObject = frame->m_slots[valueSlot];
            key = assignStringKey->m_key;
            assignType = assignStringKey->m_assignType;
            hasKey = true;
        }
        if (!hasKey) {
            Object *indexAssignOperation = Object::lookup(object, "[]=", nullptr);
            if (indexAssignOperation) {
                Object *functionBase = Object::lookup(root, "function", nullptr);
                Object *closureBase = Object::lookup(root, "closure", nullptr);

                FunctionObject *functionIndexAssignOperation =
                    (FunctionObject *)Object::instanceOf(indexAssignOperation, functionBase);
                ClosureObject *closureIndexAssignOperation =
                    (ClosureObject *)Object::instanceOf(indexAssignOperation, closureBase);

                U_ASSERT(functionIndexAssignOperation || closureIndexAssignOperation);

                Object *keyValuePair[] = { frame->m_slots[assign->m_keySlot], valueObject };
                if (functionIndexAssignOperation)
                    functionIndexAssignOperation->m_function(state, object, indexAssignOperation, keyValuePair, 2);
                else
                    closureIndexAssignOperation->m_function(state, object, indexAssignOperation, keyValuePair, 2);
                break;
            }
            U_UNREACHABLE();
        }
        switch (assignType) {
        case kAssignPlain:
            if (!object) {
                U_ASSERT(0 && "attempt to index a null");
            }
            Object::setNormal(object, key, valueObject);
            break;
        case kAssignExisting:
            Object::setExisting(object, key, valueObject);
            break;
        case kAssignShadowing:
            Object::setShadowing(object, key, valueObject);
            break;
        }
        BREAK;

    case kNewObject: BEGIN
        I(newObject, Instruction::NewObject);

        Slot targetSlot = newObject->m_targetSlot;
        Slot parentSlot = newObject->m_parentSlot;

        U_ASSERT(targetSlot < frame->m_count);
        U_ASSERT(parentSlot < frame->m_count);

        Object *parentObject = frame->m_slots[parentSlot];
        if (parentObject) {
            U_ASSERT(!(parentObject->m_flags & kNoInherit));
        }
        frame->m_slots[targetSlot] = Object::newObject(state, parentObject);
    BREAK;

    case kNewIntObject: BEGIN
        I(newIntObject, Instruction::NewIntObject);
        Slot targetSlot = newIntObject->m_targetSlot;
        int value = newIntObject->m_value;
        U_ASSERT(targetSlot < frame->m_count);
        frame->m_slots[targetSlot] = Object::newInt(state, value);
    BREAK;


    case kNewFloatObject: BEGIN
        I(newFloatObject, Instruction::NewFloatObject);
        Slot targetSlot = newFloatObject->m_targetSlot;
        float value = newFloatObject->m_value;
        U_ASSERT(targetSlot < frame->m_count);
        frame->m_slots[targetSlot] = Object::newFloat(state, value);
    BREAK;

    case kNewArrayObject: BEGIN
        I(newArrayObject, Instruction::NewArrayObject);
        Slot targetSlot = newArrayObject->m_targetSlot;
        U_ASSERT(targetSlot < frame->m_count);
        frame->m_slots[targetSlot] = Object::newArray(state, nullptr, 0);
    BREAK;

    case kNewStringObject: BEGIN
        I(newStringObject, Instruction::NewStringObject);
        Slot targetSlot = newStringObject->m_targetSlot;
        const char *value = newStringObject->m_value;
        U_ASSERT(targetSlot < frame->m_count);
        frame->m_slots[targetSlot] = Object::newString(state, value);
    BREAK;

    case kNewClosureObject: BEGIN
        I(newClosureObject, Instruction::NewClosureObject);
        Slot targetSlot = newClosureObject->m_targetSlot;
        Slot contextSlot = newClosureObject->m_contextSlot;
        U_ASSERT(targetSlot < frame->m_count);
        U_ASSERT(contextSlot < frame->m_count);
        frame->m_slots[targetSlot] = Object::newClosure(frame->m_slots[contextSlot], newClosureObject->m_function);
    BREAK;

    case kCloseObject: BEGIN
        I(closeObject, Instruction::CloseObject);
        Slot slot = closeObject->m_slot;
        U_ASSERT(slot < frame->m_count);
        Object *object = frame->m_slots[slot];
        U_ASSERT(!(object->m_flags & kClosed));
        object->m_flags |= kClosed;
        BREAK;

    case kCall: BEGIN
        I(call, Instruction::Call);

        Slot functionSlot = call->m_functionSlot;
        Slot thisSlot = call->m_thisSlot;

        size_t count = call->m_count;

        U_ASSERT(functionSlot < frame->m_count);
        U_ASSERT(thisSlot < frame->m_count);

        Object *thisObject = frame->m_slots[thisSlot];
        Object *funObject = frame->m_slots[functionSlot];

        Object *functionBase = Object::lookup(root, "function", nullptr);
        Object *closureBase = Object::lookup(root, "closure", nullptr);

        FunctionObject *functionObject = (FunctionObject *)Object::instanceOf(funObject, functionBase);
        ClosureObject *closureObject = (ClosureObject *)Object::instanceOf(funObject, closureBase);

        U_ASSERT(functionObject || closureObject);

        Object **arguments = count < 10
            ? (Object **)preallocatedArguments[count]
            : (Object **)neoMalloc(sizeof(Object *) * count);

        for (size_t i = 0; i < count; i++) {
            Slot argumentSlot = call->m_arguments[i];
            U_ASSERT(argumentSlot < frame->m_count);
            arguments[i] = frame->m_slots[argumentSlot];
        }

        if (functionObject)
            functionObject->m_function(state, thisObject, funObject, arguments, count);
        else
            closureObject->m_function(state, thisObject, funObject, arguments, count);

        if (count >= 10)
            neoFree(arguments);

        BREAK;

    case kReturn: BEGIN
        I(ret, Instruction::Return);
        Slot returnSlot = ret->m_returnSlot;
        U_ASSERT(returnSlot < frame->m_count);
        Object *result = frame->m_slots[returnSlot];
        delFrame(state);
        state->m_result = result;
        BREAK;

    case kSaveResult: BEGIN
        I(saveResult, Instruction::SaveResult);
        Slot saveSlot = saveResult->m_targetSlot;
        U_ASSERT(saveSlot < frame->m_count);
        frame->m_slots[saveSlot] = state->m_result;
        state->m_result = nullptr;
        BREAK;

    case kBranch: BEGIN
        I(branch, Instruction::Branch);
        size_t block = branch->m_block;
        U_ASSERT(block < frame->m_function->m_body.m_count);
        frame->m_block = &frame->m_function->m_body.m_blocks[block];
        frame->m_offset = -1;
        BREAK;

    case kTestBranch: BEGIN
        I(testBranch, Instruction::TestBranch);
        Slot testSlot = testBranch->m_testSlot;
        size_t trueBlock = testBranch->m_trueBlock;
        size_t falseBlock = testBranch->m_falseBlock;
        U_ASSERT(testSlot < frame->m_count);
        Object *testObject = frame->m_slots[testSlot];

        Object *boolBase = Object::lookup(root, "bool", nullptr);
        Object *intBase = Object::lookup(root, "int", nullptr);
        Object *boolObject = Object::instanceOf(testObject, boolBase);
        Object *intObject = Object::instanceOf(testObject, intBase);

        bool test = false;
        if (boolObject) {
            if (((BoolObject *)boolObject)->m_value)
                test = true;
        } else if (intObject) {
            if (((IntObject *)intObject)->m_value != 0)
                test = true;
        } else {
            test = !!testObject;
        }

        size_t targetBlock = test ? trueBlock : falseBlock;
        frame->m_block = &frame->m_function->m_body.m_blocks[targetBlock];
        frame->m_offset = -1;
        BREAK;

    default:
        U_ASSERT(0 && "invalid instruction");
        break;
    }
    frame->m_offset++;
}

void VM::run(State *state, Object *root) {
    // preallocate space for 10 arguments to place function calls
    void **arguments = (void **)neoMalloc(sizeof(void *) * 10);
    for (size_t i = 0; i < 10; i++)
        arguments[i] = neoMalloc(sizeof(Object *) * i);
    // run a bunch of steps until there is nothing left on the stack
    while (state->m_length)
        step(state, root, arguments);
}

void VM::callFunction(State *state, Object *context, UserFunction *function, Object **arguments, size_t count) {
    CallFrame *frame = addFrame(state);
    frame->m_function = function;
    frame->m_context = context;
    frame->m_count = function->m_slots;
    frame->m_slots = (Object **)neoCalloc(sizeof(Object *), frame->m_count);
    GC::addRoots(state, frame->m_slots, frame->m_count, &frame->m_root);

    U_ASSERT(count == frame->m_function->m_arity);
    for (size_t i = 0; i < count; i++)
        frame->m_slots[i] = arguments[i];

    U_ASSERT(frame->m_function->m_body.m_count);
    frame->m_block = frame->m_function->m_body.m_blocks;
    frame->m_offset = 0;
}

void VM::functionHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    (void)self;
    ClosureObject *functionObject = (ClosureObject *)function;
    callFunction(state, functionObject->m_context, &functionObject->m_closure, arguments, count);
}

void VM::methodHandler(State *state, Object *self, Object *function, Object **arguments, size_t count) {
    ClosureObject *functionObject = (ClosureObject *)function;
    Object *context = Object::newObject(state, functionObject->m_context);
    Object::setNormal(context, "this", self);
    callFunction(state, context, &functionObject->m_closure, arguments, count);
}

Object *Object::newClosure(Object *context, UserFunction *function) {
    Object *root = context;
    while (root->m_parent)
        root = root->m_parent;
    Object *closureBase = Object::lookup(root, "closure", nullptr);
    ClosureObject *closureObject = (ClosureObject *)neoCalloc(sizeof *closureObject, 1);
    closureObject->m_parent = closureBase;
    if (function->m_isMethod)
        closureObject->m_function = VM::methodHandler;
    else
        closureObject->m_function = VM::functionHandler;
    closureObject->m_context = context;
    closureObject->m_closure = *function;
    return (Object *)closureObject;
}

}
