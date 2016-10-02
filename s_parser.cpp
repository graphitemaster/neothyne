#include <string.h>
#include <stdlib.h> // atoi

#include "s_parser.h"
#include "s_gen.h"
#include "s_object.h"

#include "u_new.h"
#include "u_log.h"
#include "u_assert.h"
#include "u_vector.h"

namespace s {

#define logParseError(A, ...) \
    do { \
        u::Log::err(__VA_ARGS__); \
        u::Log::err("\n"); \
    } while (0)

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

ParseResult Parser::parseString(char **contents, char **out) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    if (*text != '"')
        return kParseNone;
    text++;
    while (*text && *text != '"') // TODO(daleweiler): string escaping
        text++;
    if (!*text) {
        logParseError(text, "expected closing quote mark");
        return kParseError;
    }
    text++;
    *contents = text;
    size_t length = text - start;
    char *result = (char *)neoMalloc(length - 2 + 1);
    memcpy(result, start + 1, length - 2);
    result[length - 2] = '\0';
    *out = result;
    return kParseOk;
}

bool Parser::consumeKeyword(char **contents, const char *keyword) {
    char *text = *contents;
    const char *compare = parseIdentifierAll(&text);
    if (!compare || strcmp(compare, keyword) != 0)
        return false;
    *contents = text;
    return kParseOk;
}

///! Parser::Reference
Slot Parser::Reference::access(Gen *gen, Reference reference) {
    // during speculative parsing there is no gen
    if (gen) {
        if (reference.m_key != NoSlot)
            return Gen::addAccess(gen, reference.m_base, reference.m_key);
        return reference.m_base;
    }
    return 0;
}

void Parser::Reference::assignPlain(Gen *gen, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    Gen::addAssign(gen, reference.m_base, reference.m_key, value, kAssignPlain);
}

void Parser::Reference::assignExisting(Gen *gen, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    Gen::addAssign(gen, reference.m_base, reference.m_key, value, kAssignExisting);
}

void Parser::Reference::assignShadowing(Gen *gen, Reference reference, Slot value) {
    U_ASSERT(reference.m_key != NoSlot);
    Gen::addAssign(gen, reference.m_base, reference.m_key, value, kAssignShadowing);
}

Parser::Reference Parser::Reference::getScope(Gen *gen, const char *name) {
    // during speculative parsing there is no gen
    if (gen) {
        Slot nameSlot = Gen::addNewStringObject(gen, gen->m_scope, name);
        return { gen->m_scope, nameSlot, kVariable };
    }
    return { 0, NoSlot, kVariable };
}

///! Parser
ParseResult Parser::parseObjectLiteral(char **contents, Gen *gen, Slot objectSlot) {
    char *text = *contents;
    while (!consumeString(&text, "}")) {
        const char *keyName = parseIdentifier(&text);
        if (!consumeString(&text, "=")) {
            logParseError(text, "object literal expects 'name = value'");
            return kParseError;
        }

        Reference value;
        ParseResult result = parseExpression(&text, gen, 0, &value);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        if (gen) {
            Slot keySlot = Gen::addNewStringObject(gen, gen->m_scope, keyName);
            Slot valueSlot = Reference::access(gen, value);
            Gen::addAssign(gen, objectSlot, keySlot, valueSlot, kAssignPlain);
        }

        if (consumeString(&text, ","))
            continue;
        if (consumeString(&text, "}"))
            break;

        logParseError(text, "expected comma or closing bracket");
        return kParseError;
    }
    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseObjectLiteral(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;

    if (!consumeString(&text, "{"))
        return kParseNone;

    Slot objectSlot = 0;
    if (gen)
        objectSlot = Gen::addNewObject(gen, gen->m_slot++);

    *contents = text;
    *reference = { objectSlot, Reference::NoSlot, Reference::kNone };

    return parseObjectLiteral(contents, gen, objectSlot);
}

ParseResult Parser::parseArrayLiteral(char **contents, Gen *gen, Slot objectSlot) {
    char *text = *contents;
    u::vector<Reference> values;
    while (!consumeString(&text, "]")) {
        Reference value;
        ParseResult result = parseExpression(&text, gen, 0, &value);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        if (gen)
            values.push_back(value);
        if (consumeString(&text, ","))
            continue;
        if (consumeString(&text, "]"))
            break;

        logParseError(text, "expected comma or closing square bracket");
        return kParseError;
    }

    *contents = text;
    if (gen) {
        Slot keySlot1 = Gen::addNewStringObject(gen, gen->m_scope, "resize");
        Slot keySlot2 = Gen::addNewStringObject(gen, gen->m_scope, "[]=");
        Slot resizeFunction = Reference::access(gen, { objectSlot, keySlot1, Reference::kObject });
        Slot assignFunction = Reference::access(gen, { objectSlot, keySlot2, Reference::kObject });
        Slot resizeSlot = Gen::addNewIntObject(gen, gen->m_scope, values.size());
        objectSlot = Gen::addCall(gen, resizeFunction, objectSlot, resizeSlot);
        for (size_t i = 0; i < values.size(); i++) {
            Slot indexSlot = Gen::addNewIntObject(gen, gen->m_scope, i);
            Gen::addCall(gen, assignFunction, objectSlot, indexSlot, Reference::access(gen, values[i]));
        }
    }

    return kParseOk;
}

ParseResult Parser::parseArrayLiteral(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    if (!consumeString(&text, "["))
        return kParseNone;

    Slot objectSlot = 0;
    if (gen)
        objectSlot = Gen::addNewArrayObject(gen, gen->m_scope);

    *contents = text;
    *reference = { objectSlot, Reference::NoSlot, Reference::kNone };

    return parseArrayLiteral(contents, gen, objectSlot);
}

ParseResult Parser::parseExpressionStem(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    const char *identifier = parseIdentifier(&text);
    if (identifier) {
        *contents = text;
        *reference = Reference::getScope(gen, identifier);
        return kParseOk;
    }

    // float?
    {
        float value = 0.0f;
        if (parseFloat(&text, &value)) {
            *contents = text;
            if (gen) {
                Slot slot = Gen::addNewFloatObject(gen, gen->m_scope, value);
                *reference = { slot, Reference::NoSlot, Reference::kNone };
            }
            return kParseOk;
        }
    }
    // int?
    {
        int value = 0;
        if (parseInteger(&text, &value)) {
            *contents = text;
            if (gen) {
                Slot slot = Gen::addNewIntObject(gen, gen->m_scope, value);
                *reference = { slot, Reference::NoSlot, Reference::kNone };
            }
            return kParseOk;
        }
    }
    // string?
    {
        ParseResult result;
        char *value = nullptr;
        if ((result = parseString(&text, &value)) != kParseNone) {
            if (result == kParseOk) {
                *contents = text;
                if (gen) {
                    Slot slot = Gen::addNewStringObject(gen, gen->m_scope, value);
                    *reference = { slot, Reference::NoSlot, Reference::kNone };
                }
            }
            return result;
        }
    }

    ParseResult result;

    // object literal
    if ((result = parseObjectLiteral(&text, gen, reference)) != kParseNone) {
        if (result == kParseOk)
            *contents = text;
        return result;
    }

    // array literal
    if ((result = parseArrayLiteral(&text, gen, reference)) != kParseNone) {
        if (result == kParseOk)
            *contents = text;
        return result;
    }

    if (consumeString(&text, "(")) {
        result = parseExpression(&text, gen, 0, reference);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        if (!consumeString(&text, ")")) {
            logParseError(text, "'()' expected closing paren");
            return kParseError;
        }

        *contents = text;
        return kParseOk;
    }

    bool isMethod = false;
    if (consumeKeyword(&text, "fn") || (consumeKeyword(&text, "method") && (isMethod = true))) {
        UserFunction *function;
        ParseResult result = parseFunctionExpression(&text, &function);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        *contents = text;

        if (gen) {
            function->m_isMethod = isMethod;
            Slot slot = Gen::addNewClosureObject(gen, gen->m_scope, function);
            *reference = { slot, Reference::NoSlot, Reference::kNone };
        }
        return kParseOk;
    }

    if (consumeKeyword(&text, "new")) {
        Reference parentVariable;
        ParseResult result = parseExpression(&text, gen, 0, &parentVariable);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        Slot parentSlot = Reference::access(gen, parentVariable);
        Slot objectSlot = 0;
        if (gen)
            objectSlot = Gen::addNewObject(gen, parentSlot);

        *contents = text;
        if (consumeString(contents, "{")) {
            ParseResult result = parseObjectLiteral(contents, gen, objectSlot);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
        }

        *reference = { objectSlot, Reference::NoSlot, Reference::kNone };
        return kParseOk;
    }

    logParseError(text, "expected expression");
    return kParseError;
}

ParseResult Parser::parseCall(char **contents, Gen *gen, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "("))
        return kParseNone;

    *contents = text;
    Slot *arguments = nullptr;
    size_t length = 0;

    while (!consumeString(&text, ")")) {
        if (length && !consumeString(&text, ",")) {
            logParseError(text, "expected comma");
            return kParseError;
        }

        Reference argument;
        ParseResult result = parseExpression(&text, gen, 0, &argument);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        Slot slot = Reference::access(gen, argument);
        arguments = (Slot *)neoRealloc(arguments, sizeof(Slot) * ++length);
        arguments[length - 1] = slot;
    }

    *contents = text;

    if (gen) {
        Slot thisSlot;
        if (expression->m_key)
            thisSlot = expression->m_base;
        else
            thisSlot = gen->m_slot++;

        *expression = {
            Gen::addCall(gen, Reference::access(gen, *expression), thisSlot, arguments, length),
            Reference::NoSlot,
            Reference::kNone
        };
    }

    return kParseOk;
}

ParseResult Parser::parseArrayAccess(char **contents, Gen *gen, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "["))
        return kParseNone;

    Reference key;
    ParseResult result = parseExpression(&text, gen, 0, &key);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    if (!consumeString(&text, "]")) {
        logParseError(*contents, "expected closing ']'");
        return kParseError;
    }

    Slot keySlot = Reference::access(gen, key);

    *contents = text;

    *expression = {
        Reference::access(gen, *expression),
        keySlot,
        Reference::kIndex
    };

    return kParseOk;
}

ParseResult Parser::parsePropertyAccess(char **contents, Gen *gen, Reference *expression) {
    char *text = *contents;
    if (!consumeString(&text, "."))
        return kParseNone;

    const char *keyName = parseIdentifier(&text);
    *contents = text;

    Slot keySlot = 0;
    if (gen)
        keySlot = Gen::addNewStringObject(gen, gen->m_scope, keyName);

    *expression = {
        Reference::access(gen, *expression),
        keySlot,
        Reference::kObject
    };

    return kParseOk;
}

ParseResult Parser::parseExpression(char **contents, Gen *gen, Reference *reference) {
    ParseResult result = parseExpressionStem(contents, gen, reference);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    for (;;) {
        if ((result = parseCall(contents, gen, reference)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        if ((result = parsePropertyAccess(contents, gen, reference)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        if ((result = parseArrayAccess(contents, gen, reference)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        break;
    }

    return kParseOk;
}

void Parser::buildOperation(Gen *gen, const char *op, Reference *lhs, Reference rhs) {
    if (gen) {
        Slot lhsSlot = Reference::access(gen, *lhs);
        Slot rhsSlot = Reference::access(gen, rhs);
        Slot function = Gen::addAccess(gen, lhsSlot, Gen::addNewStringObject(gen, gen->m_scope, op));
        *lhs = { Gen::addCall(gen, function, lhsSlot, rhsSlot), Reference::NoSlot, Reference::kNone };
    }
}

ParseResult Parser::parseExpression(char **contents, Gen *gen, int level, Reference *reference) {
    char *text = *contents;

    ParseResult result = parseExpression(&text, gen, reference);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    if (level > 2) {
        *contents = text;
        return kParseOk;
    }

    Reference expression;
    for (;;) {
        if (consumeString(&text, "*")) {
            result = parseExpression(&text, gen, 3, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "*", reference, expression);
            continue;
        }
        if (consumeString(&text, "/")) {
            result = parseExpression(&text, gen, 3, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "/", reference, expression);
            continue;
        }
        break;
    }

    if (level > 1) {
        *contents = text;
        return kParseOk;
    }

    for (;;) {
        if (consumeString(&text, "+")) {
            result = parseExpression(&text, gen, 2, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "+", reference, expression);
            continue;
        }
        if (consumeString(&text, "-")) {
            result = parseExpression(&text, gen, 2, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "-", reference, expression);
            continue;
        }
        break;
    }

    if (level > 0) {
        *contents = text;
        return kParseOk;
    }

    bool negated = false;

    if (consumeString(&text, "==")) {
        result = parseExpression(&text, gen, 1, &expression);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
        buildOperation(gen, "==", reference, expression);
    } else if (consumeString(&text, "!=")) {
        // the same code except result is negated
        result = parseExpression(&text, gen, 1, &expression);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
        buildOperation(gen, "==", reference, expression);
        negated = true;
    } else {
        // this allows for operators like !<, !>, !<=, !>=
        if (consumeString(&text, "!"))
            negated = true;
        if (consumeString(&text, "<=")) {
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "<=", reference, expression);
        } else if (consumeString(&text, ">=")) {
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, ">=", reference, expression);
        } else if (consumeString(&text, "<")) {
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "<", reference, expression);
        } else if (consumeString(&text, ">")) {
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, ">", reference, expression);
        } else if (negated) {
            logParseError(text, "expected comparison operator");
            return kParseError;
        }
    }

    if (negated && gen) {
        Slot lhs = Reference::access(gen, *reference);
        Slot bnot = Gen::addAccess(gen, lhs, Gen::addNewStringObject(gen, gen->m_scope, "!"));
        *reference = { Gen::addCall(gen, bnot, lhs), Reference::NoSlot, Reference::kNone };
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseIfStatement(char **contents, Gen *gen) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening paren after 'if'");
        return kParseError;
    }

    Reference testExpression;
    ParseResult result = parseExpression(&text, gen, 0, &testExpression);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Slot testSlot = Reference::access(gen, testExpression);
    if (!consumeString(&text, ")")) {
        logParseError(text, "expected closing paren after 'if'");
        return kParseError;
    }

    size_t *tBlock = nullptr;
    size_t *fBlock = nullptr;
    size_t *eBlock = nullptr;
    Gen::addTestBranch(gen, testSlot, &tBlock, &fBlock);

    *tBlock = Gen::newBlock(gen);

    result = parseBlock(&text, gen);
    if (result == kParseError)
        return kParseError;
    U_ASSERT(result == kParseOk);

    Gen::addBranch(gen, &eBlock);

    *fBlock = Gen::newBlock(gen);
    if (consumeKeyword(&text, "else")) {
        parseBlock(&text, gen);
        Gen::addBranch(gen, &eBlock);
        *eBlock = Gen::newBlock(gen);
    } else {
        *eBlock = *fBlock;
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseWhile(char **contents, Gen *gen) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening paren after 'while'");
        return kParseError;
    }

    size_t *tBlock = nullptr;
    size_t *lBlock = nullptr;
    size_t *eBlock = nullptr;
    size_t *oBlock = nullptr;

    Gen::addBranch(gen, &tBlock);
    *tBlock = Gen::newBlock(gen);

    Reference testExpression;
    ParseResult result = parseExpression(&text, gen, 0, &testExpression);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Slot testSlot = Reference::access(gen, testExpression);
    if (!consumeString(&text, ")")) {
        logParseError(text, "expected closing paren after 'while'");
        return kParseError;
    }

    Gen::addTestBranch(gen, testSlot, &lBlock, &eBlock);

    *lBlock = Gen::newBlock(gen);
    parseBlock(&text, gen);
    Gen::addBranch(gen, &oBlock);

    *oBlock = *tBlock;
    *eBlock = Gen::newBlock(gen);

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseReturnStatement(char **contents, Gen *gen) {
    Reference returnValue;
    ParseResult result = parseExpression(contents, gen, 0, &returnValue);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Slot value = Reference::access(gen, returnValue);
    if (!consumeString(contents, ";")) {
        logParseError(*contents, "expected semicolon");
        return kParseError;
    }

    Gen::addReturn(gen, value);
    Gen::newBlock(gen);

    return kParseOk;
}

ParseResult Parser::parseLetDeclaration(char **contents, Gen *gen) {
    char *text = *contents;

    // allocate a new scope immediately to allow recursion for closures
    // i.e allow: let foo = fn() { foo(); };
    gen->m_scope = Gen::addNewObject(gen, gen->m_scope);

    const char *variableName = parseIdentifier(&text);
    Slot value;
    Slot variableNameSlot = Gen::addNewStringObject(gen, gen->m_scope, variableName);
    if (!consumeString(&text, "=")) {
        value = gen->m_slot++;
    } else {
        Reference reference;
        ParseResult result = parseExpression(&text, gen, 0, &reference);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        value = Reference::access(gen, reference);
    }

    Gen::addAssign(gen, gen->m_scope, variableNameSlot, value, kAssignPlain);
    Gen::addCloseObject(gen, gen->m_scope);

    // let a, b
    if (consumeString(&text, ",")) {
        *contents = text;
        return parseLetDeclaration(contents, gen);
    }

    if (!consumeString(&text, ";")) {
        logParseError(text, "expected ';' to close let declaration");
        return kParseError;
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseFunctionDeclaration(char **contents, Gen *gen) {
    gen->m_scope = Gen::addNewObject(gen, gen->m_scope);

    UserFunction *function;
    ParseResult result = parseFunctionExpression(contents, &function);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Slot nameSlot = Gen::addNewStringObject(gen, gen->m_scope, function->m_name);
    Slot slot = Gen::addNewClosureObject(gen, gen->m_scope, function);
    Gen::addAssign(gen, gen->m_scope, nameSlot, slot, kAssignPlain);
    Gen::addCloseObject(gen, gen->m_scope);

    return kParseOk;
}

ParseResult Parser::parseStatement(char **contents, Gen *gen) {
    char *text = *contents;
    if (consumeKeyword(&text, "if")) {
        *contents = text;
        return parseIfStatement(contents, gen);
    }
    if (consumeKeyword(&text, "return")) {
        *contents = text;
        return parseReturnStatement(contents, gen);
    }
    if (consumeKeyword(&text, "let")) {
        *contents = text;
        return parseLetDeclaration(contents, gen);
    }
    if (consumeKeyword(&text, "fn")) {
        *contents = text;
        return parseFunctionDeclaration(contents, gen);
    }
    if (consumeKeyword(&text, "while")) {
        *contents = text;
        return parseWhile(contents, gen);
    }

    // variable assignment
    {
        char *next = text;
        Reference reference;
        ParseResult result = parseExpression(&next, nullptr, &reference); // speculative parse
        if (result == kParseError)
            return result;

        if (result == kParseOk && consumeString(&next, "=")) {
            result = parseExpression(&text, gen, &reference);
            U_ASSERT(result == kParseOk);

            if (!consumeString(&text, "=")) {
                U_ASSERT(0 && "internal compiler error");
            }

            Reference valueExpression;
            result = parseExpression(&text, gen, 0, &valueExpression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);

            Slot value = Reference::access(gen, valueExpression);

            switch (reference.m_mode) {
            case Reference::kNone:
                logParseError(text, "cannot assign to non-reference expression");
                return kParseError;
            case Reference::kVariable:
                Reference::assignExisting(gen, reference, value);
                break;
            case Reference::kObject:
                Reference::assignShadowing(gen, reference, value);
                break;
            case Reference::kIndex:
                Reference::assignPlain(gen, reference, value);
                break;
            default:
                U_ASSERT(0 && "internal compiler error");
            }

            if (!consumeString(&text, ";")) {
                logParseError(text, "expected ';' to terminate assignment statement");
                return kParseError;
            }

            *contents = text;
            return kParseOk;
        }
    }

    // expression as statement
    {
        Reference reference;
        ParseResult result = parseExpression(&text, gen, &reference);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        if (!consumeString(&text, ";")) {
            logParseError(text, "expected ';' to terminate expression");
            return kParseError;
        }

        *contents = text;
        return kParseOk;
    }

    logParseError(text, "unknown statement");
    return kParseError;
}

ParseResult Parser::parseBlock(char **contents, Gen *gen) {
    char *text = *contents;

    // Note: blocks don't actually open new scopes
    Slot currentScope = gen->m_scope;

    ParseResult result;
    if (consumeString(&text, "{")) {
        while (!consumeString(&text, "}")) {
            result = parseStatement(&text, gen);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
        }
    } else {
        result = parseStatement(&text, gen);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
    }

    *contents = text;
    gen->m_scope = currentScope;
    return kParseOk;
}

ParseResult Parser::parseFunctionExpression(char **contents, UserFunction **function) {
    char *text = *contents;
    const char *functionName = parseIdentifier(&text);

    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening paren for parameter list");
        return kParseError;
    }

    const char **arguments = nullptr;
    size_t length = 0;
    while (!consumeString(&text, ")")) {
        if (length && !consumeString(&text, ",")) {
            logParseError(text, "expected comma in parameter list");
            return kParseError;
        }
        const char *argument = parseIdentifier(&text);
        if (!argument) {
            logParseError(text, "expected identifier for parameter in parameter list");
            return kParseError;
        }
        arguments = (const char **)neoRealloc(arguments, sizeof(const char *) * ++length);
        arguments[length - 1] = argument;
    }

    *contents = text;

    Gen *gen = (Gen *)neoCalloc(sizeof *gen, 1);
    gen->m_arguments = arguments;
    gen->m_count = length;
    gen->m_slot = length;
    gen->m_name = functionName;
    gen->m_blockTerminated = true;

    // generate lexical scope
    Gen::newBlock(gen);
    Slot contextSlot = Gen::addGetContext(gen);
    gen->m_scope = Gen::addNewObject(gen, contextSlot);
    for (size_t i = 0; i < length; i++) {
        Slot argumentSlot = Gen::addNewStringObject(gen, gen->m_scope, arguments[i]);
        Gen::addAssign(gen, gen->m_scope, argumentSlot, i, kAssignPlain);
    }
    Gen::addCloseObject(gen, gen->m_scope);

    ParseResult result = parseBlock(contents, gen);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Gen::terminate(gen);

    *function = Gen::optimize(Gen::buildFunction(gen));
    neoFree(gen);
    return kParseOk;
}

ParseResult Parser::parseModule(char **contents, UserFunction **function) {
    Gen *gen = (Gen *)neoCalloc(sizeof *gen, 1);
    gen->m_slot = 0;
    gen->m_name = nullptr;
    gen->m_blockTerminated = true;

    Gen::newBlock(gen);
    gen->m_scope = Gen::addGetContext(gen);

    for (;;) {
        consumeFiller(contents);
        if ((*contents)[0] == '\0')
            break;
        ParseResult result = parseStatement(contents, gen);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
    }

    Gen::addReturn(gen, gen->m_scope);
    *function = Gen::optimize(Gen::buildFunction(gen));
    neoFree(gen);
    return kParseOk;
}

}
