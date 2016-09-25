#include <string.h>
#include <stdlib.h> // atoi

#include "s_parser.h"
#include "s_codegen.h"
#include "s_object.h"

#include "u_new.h"
#include "u_assert.h"

namespace s {

static inline bool isAlpha(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline bool isDigit (int ch) {
    return ch >= '0' && ch <= '9';
}

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

///! Parser
Parser::Reference::Reference(Slot base, const char *key)
    : m_base(base)
    , m_key(key)
{
}

Slot Parser::Reference::access(FunctionCodegen *generator) {
    return m_key ? generator->addAccess(m_base, m_key) : m_base;
}

void Parser::Reference::assignExisting(FunctionCodegen *generator, int value) {
    U_ASSERT(m_key);
    generator->addAssignExisting(m_base, m_key, value);
}

Parser::Reference Parser::Reference::getScope(FunctionCodegen *generator, const char *name) {
    return { generator ? generator->getScope() : 0, name };
}

void Parser::consumeWhitespace(char **contents) {
    while ((*contents)[0] == ' ' || (*contents)[0] == '\t')
        (*contents)++;
}

void Parser::consumeFiller(char **contents) {
    consumeWhitespace(contents);
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
    if (!strncmp(result, "fn", 2)) {
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
    if (*text && *text == '-')
        text++;
    while (*text && isDigit(*text))
        text++;
    if (text == start)
        return false;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    *out = atoi(result);
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
            if (!generator) return { 0, nullptr };
            Slot slot = generator->addAllocFloatObject(generator->getScope(), value);
            return { slot, nullptr };
        }
    }
    // int?
    {
        int value = 0;
        if (parseInteger(&text, &value)) {
            *contents = text;
            if (!generator) return { 0, nullptr };
            Slot slot = generator->addAllocIntObject(generator->getScope(), value);
            return { slot, nullptr };
        }
    }
    // string?
    {
        char *value = nullptr;
        if (parseString(&text, &value)) {
            *contents = text;
            if (!generator) return { 0, nullptr };
            Slot slot = generator->addAllocStringObject(generator->getScope(), value);
            return { slot, nullptr };
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

    if (consumeKeyword(&text, "fn")) {
        UserFunction *function = parseFunctionExpression(&text);
        Slot slot = generator->addAllocClosureObject(generator->getScope(), function);
        *contents = text;
        return { slot, nullptr };
    }

    U_ASSERT(0 && "expected expression");
    U_UNREACHABLE();
}

Parser::Reference Parser::parseExpression(char **contents, FunctionCodegen *generator) {
    Reference expression = parseExpressionStem(contents, generator);
    for (;;) {
        if (parseCall(contents, generator, &expression))
            continue;
        break;
    }
    return expression;
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
        //Slot slot = parseExpression(contents, generator, 0).access(generator);
        Reference argument = parseExpression(contents, generator, 0);
        Slot slot = generator ? argument.access(generator) : 0;
        arguments = (Slot *)neoRealloc(arguments, sizeof(Slot) * ++length);
        arguments[length - 1] = slot;
    }

    if (!generator)
        return true;

    *expression = { generator->addCall(expression->access(generator), arguments, length), nullptr };
    return true;
}

// precedence climbing technique using ordered-level splitting
//
// operator precedence table:
// 0: ==
// 1: + -
// 2: * /
Parser::Reference Parser::parseExpression(char **contents, FunctionCodegen *generator, int level) {
    char *text = *contents;

    Reference expression = parseExpression(&text, generator);

    // level 2: ( *, / )
    if (level > 2) {
        *contents = text;
        return expression;
    }

    for (;;) {
        if (consumeString(&text, "*")) {
            Slot argument = parseExpression(&text, generator, 3).access(generator);
            Slot mulFunction = generator->addAccess(generator->getScope(), "*");
            expression = { generator->addCall(mulFunction, expression.access(generator), argument), nullptr };
            continue;
        }
        if (consumeString(&text, "/")) {
            Slot argument = parseExpression(&text, generator, 3).access(generator);
            Slot divFunction = generator->addAccess(generator->getScope(), "/");
            expression = { generator->addCall(divFunction, expression.access(generator), argument), nullptr };
            continue;
        }
        break;
    }

    // level 1 ( +, - )
    if (level > 1) {
        *contents = text;
        return expression;
    }

    for (;;) {
        if (consumeString(&text, "+")) {
            Slot argument = parseExpression(&text, generator, 2).access(generator);
            Slot addFunction = generator->addAccess(generator->getScope(), "+");
            expression = { generator->addCall(addFunction, expression.access(generator), argument), nullptr };
            continue;
        }
        if (consumeString(&text, "-")) {
            Slot argument = parseExpression(&text, generator, 2).access(generator);
            Slot subFunction = generator->addAccess(generator->getScope(), "-");
            expression = { generator->addCall(subFunction, expression.access(generator), argument), nullptr };
            continue;
        }
        break;
    }

    // level 0 ( == )
    if (level > 0) {
        *contents = text;
        return expression;
    }


    if (consumeString(&text, "==")) {
        Slot argument = parseExpression(&text, generator, 2).access(generator);
        Slot equalFunction = generator->addAccess(generator->getScope(), "=");
        expression = { generator->addCall(equalFunction, expression.access(generator), argument), nullptr };
    }

    *contents = text;
    return expression;
}

void Parser::parseLetDeclaration(char **contents, FunctionCodegen *generator) {
    // allocate a new scope immediately to allow recursion for closures
    // i.e allow: let foo = fn() { foo(); };
    generator->setScope(generator->addAllocObject(generator->getScope()));

    const char *variableName = parseIdentifier(contents);
    Slot value;
    if (!consumeString(contents, "=")) {
        value = generator->makeNullSlot();
    } else {
        value = parseExpression(contents, generator, 0).access(generator);
    }

    generator->addAssign(generator->getScope(), variableName, value);
    generator->addCloseObject(generator->getScope());

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
    generator->setScope(generator->addAllocObject(generator->getScope()));
    UserFunction *function = parseFunctionExpression(contents);
    Slot slot = generator->addAllocClosureObject(generator->getScope(), function);
    generator->addAssign(generator->getScope(), function->m_name, slot);
    generator->addCloseObject(generator->getScope());
}

void Parser::parseIfStatement(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "expected open paren after if");
    }
    Slot testSlot = parseExpression(&text, generator, 0).access(generator);
    if (!consumeString(&text, ")")) {
        U_ASSERT(0 && "expected close parent after if");
    }

    size_t *trueBlock = nullptr;
    size_t *falseBlock = nullptr;
    size_t *endBlock = nullptr;
    generator->addTestBranch(testSlot, &trueBlock, &falseBlock);

    *trueBlock = generator->newBlock();
    parseBlock(&text, generator);
    generator->addBranch(&endBlock);

    *falseBlock = generator->newBlock();
    if (consumeString(&text, "else")) {
        parseBlock(&text, generator);
        generator->addBranch(&endBlock);
        *endBlock = generator->newBlock();
    } else {
        *endBlock = *falseBlock;
    }
    *contents = text;
}

void Parser::parseReturnStatement(char **contents, FunctionCodegen *generator) {
    Slot value = parseExpression(contents, generator, 0).access(generator);
    if (!consumeString(contents, ";")) {
        U_ASSERT(0 && "expected semicolon");
    }
    generator->addReturn(value);
}

void Parser::parseStatement(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (consumeKeyword(&text, "if")) {
        *contents = text;
        parseIfStatement(contents, generator);
        return;
    }
    if (consumeKeyword(&text, "fn")) {
        *contents = text;
        parseFunctionDeclaration(contents, generator);
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

    // variable assignment
    char *next = text;
    parseExpression(&next, nullptr);
    if (consumeString(&next, "=")) {
        Reference target = parseExpression(&text, generator);
        if (!consumeString(&text, "=")) {
            U_ASSERT(0 && "internal compiler error");
        }
        Slot value = parseExpression(&text, generator, 0).access(generator);
        target.assignExisting(generator, value);

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
    Slot currentScope = generator->getScope();
    if (consumeString(&text, "{")) {
        while (!consumeString(&text, "}"))
            parseStatement(&text, generator);
    } else {
        parseStatement(&text, generator);
    }
    *contents = text;
    generator->setScope(currentScope);
}

UserFunction *Parser::parseFunctionExpression(char **contents) {
    char *text = *contents;
    const char *name = parseIdentifier(&text);

    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "opening paren for argument list expected");
    }

    const char **arguments = nullptr;
    size_t length = 0;
    while (!consumeString(&text, ")")) {
        if (length && !consumeString(&text, ",")) {
            U_ASSERT(0 && "expected comma in argument list");
        }
        const char *argument = parseIdentifier(&text);
        if (!argument) {
            U_ASSERT(0 && "expected identifier in argument list");
        }
        arguments = (const char **)neoRealloc(arguments, sizeof(const char *) * ++length);
        arguments[length - 1] = argument;
    }

    *contents = text;

    FunctionCodegen generator(arguments, length, name);

    // generate lexical scope
    generator.newBlock();
    Slot contextSlot = generator.addGetContext();
    generator.setScope(generator.addAllocObject(contextSlot));
    for (size_t i = 0; i < length; i++)
        generator.addAssign(generator.getScope(), arguments[i], i);
    generator.addCloseObject(generator.getScope());

    parseBlock(contents, &generator);

    return generator.build();
}

UserFunction *Parser::parseModule(char **contents) {
    FunctionCodegen generator(0, 0, nullptr);
    generator.newBlock();
    generator.setScope(generator.addGetContext());

    for (;;) {
        consumeFiller(contents);
        if ((*contents)[0] == '\0')
            break;
        parseStatement(contents, &generator);
    }

    generator.addReturn(generator.getScope());
    return generator.build();
}

}
