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
Parser::Reference::Reference(Slot base, const char *key, bool isVariable)
    : m_base(base)
    , m_key(key)
    , m_isVariable(isVariable)
{
}

Slot Parser::Reference::access(FunctionCodegen *generator) {
    if (!generator) return 0;
    if (m_key) {
        Slot keySlot = generator->addAllocStringObject(generator->getScope(), m_key);
        return generator->addAccess(m_base, keySlot);
    }
    return m_base;
}

void Parser::Reference::assign(FunctionCodegen *generator, Slot value) {
    U_ASSERT(m_key);
    Slot keySlot = generator->addAllocStringObject(generator->getScope(), m_key);
    generator->addAssign(m_base, keySlot, value);
}

void Parser::Reference::assignExisting(FunctionCodegen *generator, Slot value) {
    U_ASSERT(m_key);
    Slot keySlot = generator->addAllocStringObject(generator->getScope(), m_key);
    generator->addAssignExisting(m_base, keySlot, value);
}

Parser::Reference Parser::Reference::getScope(FunctionCodegen *generator, const char *name) {
    return { generator ? generator->getScope() : 0, name, true };
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
            if (!generator) return { 0, nullptr, false };
            Slot slot = generator->addAllocFloatObject(generator->getScope(), value);
            return { slot, nullptr, false };
        }
    }
    // int?
    {
        int value = 0;
        if (parseInteger(&text, &value)) {
            *contents = text;
            if (!generator) return { 0, nullptr, false };
            Slot slot = generator->addAllocIntObject(generator->getScope(), value);
            return { slot, nullptr, false };
        }
    }
    // string?
    {
        char *value = nullptr;
        if (parseString(&text, &value)) {
            *contents = text;
            if (!generator) return { 0, nullptr, false };
            Slot slot = generator->addAllocStringObject(generator->getScope(), value);
            return { slot, nullptr, false };
        }
    }

    // object literal
    {
        Reference value = { 0, nullptr, false };
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

    if (consumeKeyword(&text, "fn")) {
        UserFunction *function = parseFunctionExpression(&text);
        if (!generator) return { 0, nullptr, false };
        Slot slot = generator->addAllocClosureObject(generator->getScope(), function);
        *contents = text;
        return { slot, nullptr, false };
    }

    U_ASSERT(0 && "expected expression");
    U_UNREACHABLE();
}

Parser::Reference Parser::parseExpression(char **contents, FunctionCodegen *generator) {
    Reference expression = parseExpressionStem(contents, generator);
    for (;;) {
        if (parseCall(contents, generator, &expression))
            continue;
        if (parsePropertyAccess(contents, generator, &expression))
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

    Slot thisSlot = expression->hasKey() ? expression->getSlot() : generator->makeNullSlot();

    *expression = {
        generator->addCall(expression->access(generator), thisSlot, arguments, length),
        nullptr,
        false
    };

    return true;
}

bool Parser::parsePropertyAccess(char **contents, FunctionCodegen *generator, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "."))
        return false;
    const char *name = parseIdentifier(&text);
    *contents = text;
    *expression = {
        expression->access(generator),
        name,
        false
    };
    return true;
}

bool Parser::parseObjectLiteral(char **contents, FunctionCodegen *generator, Reference *reference) {
    char *text = *contents;
    consumeFiller(&text);

    if (!consumeString(&text, "{"))
        return false;

    Slot objectSlot = generator ? generator->addAllocObject(generator->makeNullSlot()) : 0;

    while (!consumeString(&text, "}")) {
        const char *key = parseIdentifier(&text);
        if (!consumeString(&text, "=")) {
            U_ASSERT(0 && "object literal expects 'name = value'");
        }

        Reference value = parseExpression(&text, generator, 0);
        if (generator) {
            Slot keySlot = generator->addAllocStringObject(generator->getScope(), key);
            Slot valueSlot = value.access(generator);
            generator->addAssign(objectSlot, keySlot, valueSlot);
        }

        if (consumeString(&text, ","))
            continue;
        if (consumeString(&text, "}"))
            break;

        U_ASSERT(0 && "expected initializer or closing brace");
    }

    *contents = text;
    *reference = { objectSlot, nullptr, false };
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
            Slot rhsValue = parseExpression(&text, generator, 3).access(generator);
            if (!generator) continue; // speculative
            Slot lhsValue = expression.access(generator);
            Slot mulFunction = generator->addAccess(lhsValue, generator->addAllocStringObject(generator->getScope(), "*"));
            expression = {
                generator->addCall(mulFunction, lhsValue, rhsValue),
                nullptr,
                false
            };
            continue;
        }
        if (consumeString(&text, "/")) {
            Slot rhsValue = parseExpression(&text, generator, 3).access(generator);
            if (!generator) continue; // speculative
            Slot lhsValue = expression.access(generator);
            Slot divFunction = generator->addAccess(lhsValue, generator->addAllocStringObject(generator->getScope(), "/"));
            expression = {
                generator->addCall(divFunction, lhsValue, rhsValue),
                nullptr,
                false
            };
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
            Slot rhsValue = parseExpression(&text, generator, 2).access(generator);
            if (!generator) continue; // speculative
            Slot lhsValue = expression.access(generator);
            Slot addFunction = generator->addAccess(lhsValue, generator->addAllocStringObject(generator->getScope(), "+"));
            expression = {
                generator->addCall(addFunction, lhsValue, rhsValue),
                nullptr,
                false
            };
            continue;
        }
        if (consumeString(&text, "-")) {
            Slot rhsValue = parseExpression(&text, generator, 2).access(generator);
            if (!generator) continue; // speculaitve
            Slot lhsValue = expression.access(generator);
            Slot subFunction = generator->addAccess(lhsValue, generator->addAllocStringObject(generator->getScope(), "-"));
            expression = {
                generator->addCall(subFunction, lhsValue, rhsValue),
                nullptr,
                false
            };
            continue;
        }
        break;
    }

    // level 0 ( == )
    if (level > 0) {
        *contents = text;
        return expression;
    }

    // comparison operators
    bool negate = false;
    if (consumeString(&text, "==")) {
        Slot rhs = parseExpression(&text, generator, 1).access(generator);
        if (generator) {
            Slot lhs = expression.access(generator);
            Slot equalFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), "=="));
            expression = { generator->addCall(equalFunction, lhs, rhs), nullptr, false };
        }
    } else if (consumeString(&text, "!=")) {
        Slot rhs = parseExpression(&text, generator, 1).access(generator);
        if (generator) {
            Slot lhs = expression.access(generator);
            Slot equalFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), "=="));
            expression = { generator->addCall(equalFunction, lhs, rhs), nullptr, false };
            negate = true;
        }
    } else {
        if (consumeString(&text, "!"))
            negate = true;
        if (consumeString(&text, ">=")) {
            Slot rhs = parseExpression(&text, generator, 1).access(generator);
            if (generator) {
                Slot lhs = expression.access(generator);
                Slot geFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), ">="));
                expression = { generator->addCall(geFunction, lhs, rhs), nullptr, false };
            }
        } else if (consumeString(&text, "<=")) {
            Slot rhs = parseExpression(&text, generator, 1).access(generator);
            if (generator) {
                Slot lhs = expression.access(generator);
                Slot leFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), "<="));
                expression = { generator->addCall(leFunction, lhs, rhs), nullptr, false };
            }
        } else if (consumeString(&text, ">")) {
            Slot rhs = parseExpression(&text, generator, 1).access(generator);
            if (generator) {
                Slot lhs = expression.access(generator);
                Slot gtFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), ">"));
                expression = { generator->addCall(gtFunction, lhs, rhs), nullptr, false };
            }
        } else if (consumeString(&text, "<")) {
            Slot rhs = parseExpression(&text, generator, 1).access(generator);
            if (generator) {
                Slot lhs = expression.access(generator);
                Slot leFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), "<"));
                expression = { generator->addCall(leFunction, lhs, rhs), nullptr, false };
            }
        } else if (negate) {
            U_ASSERT(0 && "expected comparison operator");
        }
    }

    if (negate && generator) {
        Slot lhs = expression.access(generator);
        Slot notFunction = generator->addAccess(lhs, generator->addAllocStringObject(generator->getScope(), "!"));
        expression = { generator->addCall(notFunction, lhs), nullptr, false };
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
    Slot slot = generator->addAllocStringObject(generator->getScope(), variableName);
    if (!consumeString(contents, "=")) {
        value = generator->makeNullSlot();
    } else {
        value = parseExpression(contents, generator, 0).access(generator);
    }

    generator->addAssign(generator->getScope(), slot, value);
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
    Slot nameSlot = generator->addAllocStringObject(generator->getScope(), function->m_name);
    Slot slot = generator->addAllocClosureObject(generator->getScope(), function);
    generator->addAssign(generator->getScope(), nameSlot, slot);
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
        if (target.isVariable())
            target.assignExisting(generator, value);
        else
            target.assign(generator, value);

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
    for (size_t i = 0; i < length; i++) {
        Slot argumentNameSlot = generator.addAllocStringObject(generator.getScope(), arguments[i]);
        generator.addAssign(generator.getScope(), argumentNameSlot, i);
    }
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
