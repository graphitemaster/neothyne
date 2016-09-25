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

void Parser::consumeWhitespace(char **contents) {
    while ((*contents)[0] == ' ' || (*contents)[0] == '\t')
        (*contents)++;
}

bool Parser::consumeString(char **contents, const char *identifier) {
    char *text = *contents;
    consumeWhitespace(&text);
    if (startsWith(&text, identifier)) {
        *contents = text;
        return true;
    }
    return false;
}

// returns memory which can leak
const char *Parser::parseIdentifier(char **contents) {
    char *text = *contents;
    consumeWhitespace(&text);
    char *start = text;
    if (*text && (isAlpha(*text) || *text == '_'))
        text++;
    else return nullptr;
    while (*text && (isAlpha(*text) || isDigit(*text) || *text == '_'))
        text++;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    return result;
}

bool Parser::parseInteger(char **contents, int *out) {
    char *text = *contents;
    consumeWhitespace(&text);
    char *start = text;
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

bool Parser::consumeKeyword(char **contents, const char *keyword) {
    char *text = *contents;
    const char *compare = parseIdentifier(&text);
    if (!compare || strcmp(compare, keyword) != 0)
        return false;
    *contents = text;
    return true;
}

Slot Parser::parseExpression(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    const char *identifier = parseIdentifier(&text);
    if (identifier) {
        *contents = text;
        return generator->addAccess(generator->getScope(), identifier);
    }

    int value = 0;
    if (parseInteger(&text, &value)) {
        *contents = text;
        Slot contextSlot = generator->addGetContext();
        // TODO(daleweiler): pass context
        (void)contextSlot;
        return generator->addAllocIntObject(value);
    }

    if (consumeString(&text, "(")) {
        Slot result = parseExpression(&text, generator, 0);
        if (!consumeString(&text, ")")) {
            U_ASSERT(0 && "expected closing paren");
        }
        *contents = text;
        return result;
    }

    U_ASSERT(0 && "expected identifier, number or expression");
    U_UNREACHABLE();
}

bool Parser::parseCall(char **contents, FunctionCodegen *generator, Slot *expression) {
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
        Slot slot = parseExpression(contents, generator, 0);
        arguments = (Slot *)neoRealloc(arguments, sizeof(Slot) * ++length);
        arguments[length - 1] = slot;
    }
    *expression = generator->addCall(*expression, arguments, length);
    return true;
}

Slot Parser::parseExpressionTail(char **contents, FunctionCodegen *generator) {
    Slot expression = parseExpression(contents, generator);
    for (;;) {
        if (parseCall(contents, generator, &expression))
            continue;
        break;
    }
    return expression;
}

// precedence climbing technique using ordered-level splitting
//
// operator precedence table:
// 0: ==
// 1: + -
// 2: * /
Slot Parser::parseExpression(char **contents, FunctionCodegen *generator, int level) {
    char *text = *contents;

    Slot expression = parseExpressionTail(&text, generator);

    // level 2: ( *, / )
    if (level > 2) {
        *contents = text;
        return expression;
    }

    for (;;) {
        if (consumeString(&text, "*")) {
            Slot argument = parseExpression(&text, generator, 3);
            Slot contextSlot = generator->addGetContext();
            Slot mulFunction = generator->addAccess(contextSlot, "*");
            expression = generator->addCall(mulFunction, expression, argument);
            continue;
        }
        if (consumeString(&text, "/")) {
            Slot argument = parseExpression(&text, generator, 3);
            Slot contextSlot = generator->addGetContext();
            Slot divFunction = generator->addAccess(contextSlot, "/");
            expression = generator->addCall(divFunction, expression, argument);
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
            Slot argument = parseExpression(&text, generator, 2);
            Slot contextSlot = generator->addGetContext();
            Slot addFunction = generator->addAccess(contextSlot, "+");
            expression = generator->addCall(addFunction, expression, argument);
            continue;
        }
        if (consumeString(&text, "-")) {
            Slot argument = parseExpression(&text, generator, 2);
            Slot contextSlot = generator->addGetContext();
            Slot subFunction = generator->addAccess(contextSlot, "-");
            expression = generator->addCall(subFunction, expression, argument);
            continue;
        }
        break;
    }

    // level 0 ( == )
    if (level > 0) {
        *contents = text;
        return expression;
    }

    for (;;) {
        if (consumeString(&text, "==")) {
            Slot argument = parseExpression(&text, generator, 2);
            Slot contextSlot = generator->addGetContext();
            Slot equalFunction = generator->addAccess(contextSlot, "=");
            expression = generator->addCall(equalFunction, expression, argument);
            continue;
        }
        break;
    }

    *contents = text;
    return expression;
}

void Parser::parseIfStatement(char **contents, FunctionCodegen *generator) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        U_ASSERT(0 && "expected open paren after if");
    }
    Slot testSlot = parseExpression(&text, generator, 0);
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
    Slot value = parseExpression(contents, generator, 0);
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
    if (consumeKeyword(&text, "return")) {
        *contents = text;
        parseReturnStatement(contents, generator);
        return;
    }
    U_ASSERT(0 && "unknown statement");
    U_UNREACHABLE();
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

UserFunction *Parser::parseFunction(char **contents) {
    char *text = *contents;
    if (!consumeKeyword(&text, "fn"))
        return nullptr;
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

}
