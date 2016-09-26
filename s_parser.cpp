#include <string.h>
#include <stdlib.h> // atoi

#include "s_parser.h"
#include "s_codegen.h"
#include "s_object.h"

#include "u_new.h"
#include "u_assert.h"

namespace s {

static bool startsWith(char **contents, const char *compare) {
    char *text = *contents;
    while (*compare) {
        if (!*compare)
            return false;
        if (*text != *compare)
            return false;
        text++;
        compare++;
    }
    *contents = text;
    return true;
}

static inline bool isAlpha(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline bool isDigit (int ch) {
    return ch >= '0' && ch <= '9';
}

void Parser::consumeFiller(char **contents) {
    size_t commentDepth = 0;
    while (**contents) {
        if (commentDepth) {
            if (startsWith(contents, "*/")) {
                commentDepth--;
            } else {
                (*contents)++;
            }
        } else {
            if (startsWith(contents, "/*")) {
                commentDepth++;
            } else if (startsWith(contents, "//")) {
                while (**contents && **contents != '\n')
                    (*contents)++;
            } else if (**contents == ' ' || **contents == '\n') {
                (*contents)++;
            } else {
                break;
            }
        }
    }
}

bool Parser::consumeString(char **contents, const char *identifier) {
    char *text = *contents;
    consumeFiller(&text);
    if (startsWith(&text, identifier)) {
        *contents = text;
        return true;
    }
    return false;
}

// returns memory which can leak
const char *Parser::parseIdentifierAll(char **contents) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    if (*text && (isAlpha(*text) || *text == '_'))
        text++;
    else return nullptr;
    while (*text && (isAlpha(*text) || isDigit(*text) || *text == '_'))
        text++;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    *contents = text;
    return result;
}

const char *Parser::parseIdentifier(char **contents) {
    char *text = *contents;
    const char *result = parseIdentifierAll(&text);
    if (!result)
        return nullptr;
    if (!strncmp(result, "fn", 2) || !strncmp(result, "method", 6) || !strcmp(result, "new")) {
        // reserved identifier
        neoFree((char *)result);
        return nullptr;
    }
    *contents = text;
    return result;
}

bool Parser::parseInteger(char **contents, int *out) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    int base = 10;
    if (*text && *text == '-')
        text++;
    if (text[0] == '0' && text[1] == 'x') {
        base = 16;
        text += 2;
    }
    while (*text) {
        if (base >= 10 && isDigit(*text))
            text++;
        else if (base >= 16 && isAlpha(*text))
            text++;
        else break;
    }
    if (text == start)
        return false;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    *out = strtol(result, nullptr, base);
    neoFree(result);
    return true;
}

bool Parser::parseFloat(char **contents, float *out) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    if (*text && *text == '-')
        text++;
    while (*text && isDigit(*text))
        text++;
    if (!*text || *text != '.')
        return false;
    text++;
    while (*text && isDigit(*text))
        text++;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length + 1);
    memcpy(result, start, length);
    *out = atof(result);
    neoFree(result);
    return true;
}

bool Parser::parseString(char **contents, char **out) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    if (*text != '"')
        return false;
    text++;
    while (*text != '"') // TODO(daleweiler): string escaping
        text++;
    text++;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length - 2 + 1);
    memcpy(result, start + 1, length - 2);
    result[length - 2] = '\0';
    *out = result;
    return true;
}

bool Parser::consumeKeyword(char **contents, const char *keyword) {
    char *text = *contents;
    const char *compare = parseIdentifierAll(&text);
    if (!compare || strcmp(compare, keyword) != 0)
        return false;
    *contents = text;
    return true;
}

///! Parser::Reference
Slot Parser::Reference::access(FunctionCodegen *generator, Reference reference) {
    // during speculative parsing there is no generator
    if (generator) {
        if (reference.m_key != NoSlot)
            return generator->addAccess(reference.m_base, reference.m_key);
        return reference.m_base;
    }
    return 0;
}

void Parser::Reference::assignNormal(FunctionCodegen *generator, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    generator->addAssignNormal(reference.m_base, reference.m_key, value);
}

void Parser::Reference::assignExisting(FunctionCodegen *generator, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    generator->addAssignExisting(reference.m_base, reference.m_key, value);
}

void Parser::Reference::assignShadowing(FunctionCodegen *generator, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    generator->addAssignShadowing(reference.m_base, reference.m_key, value);
}

Parser::Reference Parser::Reference::getScope(FunctionCodegen *generator, const char *name) {
    // during speculative parsing there is no generator
    if (generator) {
        Slot nameSlot = generator->addAllocStringObject(generator->m_scope, name);
        return { generator->m_scope, nameSlot, kVariable };
    }
    return { 0, NoSlot, kVariable };
}

///! Parser
void Parser::parseObjectLiteral(char **contents, FunctionCodegen *generator, Slot objectSlot) {
    while (!consumeString(contents, "}")) {
        const char *keyName = parseIdentifier(contents);
        if (!consumeString(contents, "=")) {
            U_ASSERT(0 && "expected 'name = value' in object literal");
        }

        Reference value = parseExpression(contents, generator, 0);
        if (generator) {
            Slot keySlot = generator->addAllocStringObject(generator->m_scope, keyName);
            Slot valueSlot = Reference::access(generator, value);
            generator->addAssignNormal(objectSlot, keySlot, valueSlot);
        }

        if (consumeString(contents, ","))
            continue;
        if (consumeString(contents, "}"))
            break;

        U_ASSERT(0 && "expected comma or closing brace in object literal");
    }
}

bool Parser::parseObjectLiteral(char **contents, FunctionCodegen *generator, Reference *reference) {
    char *text = *contents;
    consumeFiller(&text);

    if (!consumeString(&text, "{"))
        return false;

    Slot objectSlot = 0;
    if (generator)
        objectSlot = generator->addAllocObject(generator->m_slotBase++);

    *contents = text;
    *reference = { objectSlot, Reference::NoSlot, Reference::kNone };

    parseObjectLiteral(contents, generator, objectSlot);

    return true;
}

Parser::Reference Parser::parseExpressionStem(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    const char *identifier = parseIdentifier(&text);
    if (identifier) {
        *contents = text;
        return Reference::getScope(generator, identifier);
    }

    // float?
    {
        float value = 0.0f;
        if (parseFloat(&text, &value)) {
            *contents = text;
            if (!generator)
                return { 0, Reference::NoSlot, Reference::kNone };
            Slot slot = generator->addAllocFloatObject(generator->m_scope, value);
            return { slot, Reference::NoSlot, Reference::kNone };
        }
    }
    // int?
    {
        int value = 0;
        if (parseInteger(&text, &value)) {
            *contents = text;
            if (!generator)
                return { 0, Reference::NoSlot, Reference::kNone };
            Slot slot = generator->addAllocIntObject(generator->m_scope, value);
            return { slot, Reference::NoSlot, Reference::kNone };
        }
    }
    // string?
    {
        char *value = nullptr;
        if (parseString(&text, &value)) {
            *contents = text;
            if (!generator)
                return { 0, Reference::NoSlot, Reference::kNone };
            Slot slot = generator->addAllocStringObject(generator->m_scope, value);
            return { slot, Reference::NoSlot, Reference::kNone };
        }
    }

    // object literal
    {
        Reference value;
        if (parseObjectLiteral(&text, generator, &value)) {
            *contents = text;
            return value;
        }
    }

    if (consumeString(&text, "(")) {
        Reference result = parseExpression(&text, generator, 0);
        if (!consumeString(&text, ")")) {
            U_ASSERT(0 && "expected closing paren");
        }
        *contents = text;
        return result;
    }

    bool isMethod = false;
    if (consumeKeyword(&text, "fn") || (consumeKeyword(&text, "method") && (isMethod = true))) {
        UserFunction *function = parseFunctionExpression(&text);
        if (!generator) return { 0, Reference::NoSlot, Reference::kNone };
        function->m_isMethod = isMethod;
        Slot slot = generator->addAllocClosureObject(generator->m_scope, function);
        *contents = text;
        return { slot, Reference::NoSlot, Reference::kNone };
    }

    if (consumeKeyword(&text, "new")) {
        Reference parentVariable = parseExpression(&text, generator, 0);
        Slot parentSlot = Reference::access(generator, parentVariable);
        Slot objectSlot = 0;
        if (generator)
            objectSlot = generator->addAllocObject(parentSlot);

        *contents = text;
        if (consumeString(contents, "{"))
            parseObjectLiteral(contents, generator, objectSlot);

        return { objectSlot, Reference::NoSlot, Reference::kNone };
    }

    U_ASSERT(0 && "expected expression");
    U_UNREACHABLE();
}

bool Parser::parseCall(char **contents, FunctionCodegen *generator, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "("))
        return false;

    *contents = text;
    Slot *arguments = nullptr;
    size_t length = 0;

    while (!consumeString(contents, ")")) {
        if (length && !consumeString(contents, ",")) {
            U_ASSERT(0 && "expected comma");
        }

        Reference argument = parseExpression(contents, generator, 0);
        Slot slot = Reference::access(generator, argument);
        arguments = (Slot *)neoRealloc(arguments, sizeof(Slot) * ++length);
        arguments[length - 1] = slot;
    }

    if (!generator)
        return true;

    // generate an empty slot if there isn't a key
    Slot thisSlot;
    if (expression->m_key)
        thisSlot = expression->m_base;
    else
        thisSlot = generator->m_slotBase++;

    *expression = {
        generator->addCall(Reference::access(generator, *expression), thisSlot, arguments, length),
        Reference::NoSlot,
        Reference::kNone
    };

    return true;
}

bool Parser::parseArrayAccess(char **contents, FunctionCodegen *generator, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "["))
        return false;

    *contents = text;
    Reference key = parseExpression(contents, generator, 0);

    if (!consumeString(contents, "]")) {
        U_ASSERT(0 && "expected closing ']'");
    }

    Slot keySlot = Reference::access(generator, key);

    *expression = {
        Reference::access(generator, *expression),
        keySlot,
        Reference::kIndex
    };

    return true;
}

bool Parser::parsePropertyAccess(char **contents, FunctionCodegen *generator, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "."))
        return false;

    const char *keyName = parseIdentifier(&text);
    *contents = text;

    Slot keySlot = 0;
    if (generator)
        keySlot = generator->addAllocStringObject(generator->m_scope, keyName);

    *expression = {
        Reference::access(generator, *expression),
        keySlot,
        Reference::kObject
    };

    return true;
}

Parser::Reference Parser::parseExpression(char **contents, FunctionCodegen *generator) {
    Reference expression = parseExpressionStem(contents, generator);
    for (;;) {
        if (parseCall(contents, generator, &expression))
            continue;
        if (parsePropertyAccess(contents, generator, &expression))
            continue;
        if (parseArrayAccess(contents, generator, &expression))
            continue;
        break;
    }
    return expression;
}

#define GUARD_LEVEL(LEVEL) \
    do { \
        if (level > (LEVEL)) { \
            *contents = text; \
            return expression; \
        } \
    } while (0)

#define GUARD_SPECULATIVE() \
    if (!generator) continue;

Parser::Reference Parser::parseExpression(char **contents, FunctionCodegen *generator, int level) {
    char *text = *contents;
    Reference expression = parseExpression(&text, generator);

    GUARD_LEVEL(2);
    for (;;) {
        if (consumeString(&text, "*")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 3));
            GUARD_SPECULATIVE();
            Slot lhs = Reference::access(generator, expression);
            Slot mul = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "*"));
            expression = { generator->addCall(mul, lhs, rhs), Reference::NoSlot, Reference::kNone };
            continue;
        }
        if (consumeString(&text, "/")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 3));
            GUARD_SPECULATIVE();
            Slot lhs = Reference::access(generator, expression);
            Slot div = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "/"));
            expression = { generator->addCall(div, lhs, rhs), Reference::NoSlot, Reference::kNone };
            continue;
        }
        break;
    }

    GUARD_LEVEL(1);
    for (;;) {
        if (consumeString(&text, "+")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 3));
            GUARD_SPECULATIVE();
            Slot lhs = Reference::access(generator, expression);
            Slot add = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "+"));
            expression = { generator->addCall(add, lhs, rhs), Reference::NoSlot, Reference::kNone };
            continue;
        }
        if (consumeString(&text, "-")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 3));
            GUARD_SPECULATIVE();
            Slot lhs = Reference::access(generator, expression);
            Slot sub = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "-"));
            expression = { generator->addCall(sub, lhs, rhs), Reference::NoSlot, Reference::kNone };
            continue;
        }
        break;
    }

    GUARD_LEVEL(0);
    bool negate = false;
    if (consumeString(&text, "==")) {
        Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
        if (generator) {
            Slot lhs = Reference::access(generator, expression);
            Slot equ = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "=="));
            expression = { generator->addCall(equ, lhs, rhs), Reference::NoSlot, Reference::kNone };
        }
    } else if (consumeString(&text, "!=")) {
        // lhs != rhs is implemented as !(lhs == rhs)
        Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
        if (generator) {
            Slot lhs = Reference::access(generator, expression);
            Slot equ = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "=="));
            expression = { generator->addCall(equ, lhs, rhs), Reference::NoSlot, Reference::kNone };
            negate = true;
        }
    } else {
        if (consumeString(&text, "!"))
            negate = true;
        if (consumeString(&text, "<=")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
            if (generator) {
                Slot lhs = Reference::access(generator, expression);
                Slot lte = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "<="));
                expression = { generator->addCall(lte, lhs, rhs), Reference::NoSlot, Reference::kNone };
            }
        } else if (consumeString(&text, ">=")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
            if (generator) {
                Slot lhs = Reference::access(generator, expression);
                Slot gte = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, ">="));
                expression = { generator->addCall(gte, lhs, rhs), Reference::NoSlot, Reference::kNone };
            }
        } else if (consumeString(&text, "<")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
            if (generator) {
                Slot lhs = Reference::access(generator, expression);
                Slot lt = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "<"));
                expression = { generator->addCall(lt, lhs, rhs), Reference::NoSlot, Reference::kNone };
            }
        } else if (consumeString(&text, ">")) {
            Slot rhs = Reference::access(generator, parseExpression(&text, generator, 1));
            if (generator) {
                Slot lhs = Reference::access(generator, expression);
                Slot gt = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, ">"));
                expression = { generator->addCall(gt, lhs, rhs), Reference::NoSlot, Reference::kNone };
            }
        } else if (negate) {
            U_ASSERT(0 && "expected relational operator");
        }
    }

    if (negate && generator) {
        Slot lhs = Reference::access(generator, expression);
        Slot bnot = generator->addAccess(lhs, generator->addAllocStringObject(generator->m_scope, "!"));
        expression = { generator->addCall(bnot, lhs), Reference::NoSlot, Reference::kNone };
    }

    *contents = text;
    return expression;
}

void Parser::parseIfStatement(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "expected open paren after if");
    }

    Slot testSlot = Reference::access(generator, parseExpression(&text, generator, 0));
    if (!consumeString(&text, ")")) {
        U_ASSERT(0 && "expected close parent after if");
    }

    Block *tBlock = nullptr;
    Block *fBlock = nullptr;
    Block *eBlock = nullptr;
    generator->addTestBranch(testSlot, &tBlock, &fBlock);

    *tBlock = generator->newBlock();
    parseBlock(&text, generator);
    generator->addBranch(&eBlock);

    *fBlock = generator->newBlock();
    if (consumeKeyword(&text, "else")) {
        parseBlock(&text, generator);
        generator->addBranch(&eBlock);
        *eBlock = generator->newBlock();
    } else {
        *eBlock = *fBlock;
    }
    *contents = text;
}

void Parser::parseWhile(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "expected openening parenthesis after 'while'");
    }

    Block *tBlock = nullptr;
    Block *lBlock = nullptr;
    Block *eBlock = nullptr;
    Block *oBlock = nullptr;

    generator->addBranch(&tBlock);
    *tBlock = generator->newBlock();
    Slot testSlot = Reference::access(generator, parseExpression(&text, generator, 0));
    if (!consumeString(&text, ")")) {
        U_ASSERT(0 && "expected closing parenthesis after 'while'");
    }

    generator->addTestBranch(testSlot, &lBlock, &eBlock);

    *lBlock = generator->newBlock();
    parseBlock(&text, generator);
    generator->addBranch(&oBlock);

    *oBlock = *tBlock;
    *eBlock = generator->newBlock();

    *contents = text;
}

void Parser::parseReturnStatement(char **contents, FunctionCodegen *generator) {
    Slot value = Reference::access(generator, parseExpression(contents, generator, 0));
    if (!consumeString(contents, ";")) {
        U_ASSERT(0 && "expected semicolon");
    }
    generator->addReturn(value);
    generator->newBlock();
}

void Parser::parseLetDeclaration(char **contents, FunctionCodegen *generator) {
    // allocate a new scope immediately to allow recursion for closures
    // i.e allow: let foo = fn() { foo(); };
    generator->m_scope = generator->addAllocObject(generator->m_scope);

    const char *variableName = parseIdentifier(contents);
    Slot value;
    Slot variableNameSlot = generator->addAllocStringObject(generator->m_scope, variableName);
    if (!consumeString(contents, "=")) {
        value = generator->m_slotBase++;
    } else {
        value = Reference::access(generator, parseExpression(contents, generator, 0));
    }

    generator->addAssignNormal(generator->m_scope, variableNameSlot, value);
    generator->addCloseObject(generator->m_scope);

    // let a, b
    if (consumeString(contents, ",")) {
        parseLetDeclaration(contents, generator);
        return;
    }

    if (!consumeString(contents, ";")) {
        U_ASSERT(0 && "expected `;' to terminate `let' declaration");
    }
}

void Parser::parseFunctionDeclaration(char **contents, FunctionCodegen *generator) {
    generator->m_scope = generator->addAllocObject(generator->m_scope);

    UserFunction *function = parseFunctionExpression(contents);
    Slot nameSlot = generator->addAllocStringObject(generator->m_scope, function->m_name);
    Slot slot = generator->addAllocClosureObject(generator->m_scope, function);
    generator->addAssignNormal(generator->m_scope, nameSlot, slot);
    generator->addCloseObject(generator->m_scope);
}

void Parser::parseStatement(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (consumeKeyword(&text, "if")) {
        *contents = text;
        parseIfStatement(contents, generator);
        return;
    }
    if (consumeKeyword(&text, "return")) {
        *contents = text;
        parseReturnStatement(contents, generator);
        return;
    }
    if (consumeKeyword(&text, "let")) {
        *contents = text;
        parseLetDeclaration(contents, generator);
        return;
    }
    if (consumeKeyword(&text, "fn")) {
        *contents = text;
        parseFunctionDeclaration(contents, generator);
        return;
    }
    if (consumeKeyword(&text, "while")) {
        *contents = text;
        parseWhile(contents, generator);
        return;
    }

    // variable assignment
    char *next = text;
    parseExpression(&next, nullptr);
    if (consumeString(&next, "=")) {
        Reference target = parseExpression(&text, generator);

        if (!consumeString(&text, "=")) {
            U_ASSERT(0 && "internal compiler error");
        }

        Slot value = Reference::access(generator, parseExpression(&text, generator, 0));

        switch (target.m_mode) {
        case Reference::kVariable:
            Reference::assignExisting(generator, target, value);
            break;
        case Reference::kObject:
            Reference::assignShadowing(generator, target, value);
            break;
        case Reference::kIndex:
            Reference::assignNormal(generator, target, value);
            break;
        default:
            U_ASSERT(0 && "internal compiler error");
        }

        if (!consumeString(&text, ";")) {
            U_ASSERT(0 && "expected `;' to close assignment");
        }

        *contents = text;
        return;
    }

    // expression as statement
    parseExpression(&text, generator);
    if (!consumeString(&text, ";")) {
        U_ASSERT(0 && "expected `;' to close expression");
    }
    *contents = text;
}

void Parser::parseBlock(char **contents, FunctionCodegen *generator) {
    char *text = *contents;

    // Note: blocks don't actually open new scopes
    Slot currentScope = generator->m_scope;

    if (consumeString(&text, "{")) {
        while (!consumeString(&text, "}"))
            parseStatement(&text, generator);
    } else {
        parseStatement(&text, generator);
    }

    *contents = text;
    generator->m_scope = currentScope;
}

UserFunction *Parser::parseFunctionExpression(char **contents) {
    char *text = *contents;
    const char *functionName = parseIdentifier(&text);

    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "opening paren for parameter list expected");
    }

    const char **arguments = nullptr;
    size_t length = 0;
    while (!consumeString(&text, ")")) {
        if (length && !consumeString(&text, ",")) {
            U_ASSERT(0 && "expected comma in parameter list");
        }
        const char *argument = parseIdentifier(&text);
        if (!argument) {
            U_ASSERT(0 && "expected identifier in parameter list");
        }
        arguments = (const char **)neoRealloc(arguments, sizeof(const char *) * ++length);
        arguments[length - 1] = argument;
    }

    *contents = text;

    FunctionCodegen *generator = (FunctionCodegen *)memset(neoMalloc(sizeof *generator), 0, sizeof *generator);
    generator->m_arguments = arguments;
    generator->m_length = length;
    generator->m_slotBase = length;
    generator->m_name = functionName;
    generator->m_terminated = true;

    // generate lexical scope
    generator->newBlock();
    Slot contextSlot = generator->addGetContext();
    generator->m_scope = generator->addAllocObject(contextSlot);
    for (size_t i = 0; i < length; i++) {
        Slot argumentSlot = generator->addAllocStringObject(generator->m_scope, arguments[i]);
        generator->addAssignNormal(generator->m_scope, argumentSlot, i);
    }
    generator->addCloseObject(generator->m_scope);

    parseBlock(contents, generator);
    generator->terminate();

    return generator->build();
}

UserFunction *Parser::parseModule(char **contents) {
    FunctionCodegen *generator = (FunctionCodegen *)memset(neoMalloc(sizeof *generator), 0, sizeof *generator);
    generator->m_slotBase = 0;
    generator->m_name = nullptr;
    generator->m_terminated = true;

    generator->newBlock();
    generator->m_scope = generator->addGetContext();

    for (;;) {
        consumeFiller(contents);
        if ((*contents)[0] == '\0')
            break;
        parseStatement(contents, generator);
    }

    generator->addReturn(generator->m_scope);
    return generator->build();
}

}
