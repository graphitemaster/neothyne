#include <string.h>
#include <stdlib.h> // atoi

#include "s_parser.h"
#include "s_gen.h"
#include "s_object.h"
#include "s_memory.h"

#include "u_new.h"
#include "u_log.h"
#include "u_assert.h"
#include "u_vector.h"

namespace s {

static void logParseError(char *location, const char *format, ...) {
    SourceRange line;
    const char *file;
    int row;
    int col;

    auto utf8len = [](const char *ptr, size_t length) {
        size_t len = 0;
        for (const char *end = ptr + length; ptr != end; ptr++)
            if ((*ptr & 0xC0) != 0x80) len++;
        return len;
    };

    va_list va;
    va_start(va, format);
    if (SourceRecord::findSourcePosition(location, &file, &line, &row, &col)) {
        u::Log::err("%s:%i:%i: error: %s\n", file, row + 1, col + 1, u::formatProcess(format, va));
        u::Log::err("%.*s", (int)(line.m_end - line.m_begin), line.m_begin);
        int u8col = utf8len(line.m_begin, col);
        int u8len = utf8len(line.m_begin, line.m_end - line.m_begin);
        for (int i = 0; i < u8len; i++) {
            if (i < u8col)
                u::Log::err(" ");
            else if (i == u8col)
                u::Log::err("^");
        }
        u::Log::err("\n");
    } else {
        u::Log::err("error: %.*s %s\n", 20, location, u::formatProcess(format, va));
    }
    va_end(va);
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
    char *result = (char *)Memory::allocate(length + 1);
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
    if (!strcmp(result, "fn") || !strcmp(result, "method") || !strcmp(result, "new")) {
        Memory::free((void *)result);
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
    char *result = (char *)Memory::allocate(length + 1);
    memcpy(result, start, length);
    result[length] = '\0';
    *out = strtol(result, nullptr, base);
    Memory::free(result);
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
    char *result = (char *)Memory::allocate(length + 1);
    memcpy(result, start, length);
    *out = atof(result);
    Memory::free(result);
    return true;
}

ParseResult Parser::parseString(char **contents, char **out) {
    char *text = *contents;
    consumeFiller(&text);
    char *start = text;
    if (*text != '"')
        return kParseNone;
    text++;
    int escaped = 0;
    while (*text && *text != '"') {
        if (*text == '\\') {
            text++;
            if (!*text) {
                logParseError(text, "unterminated escape");
                return kParseError;
            }
        }
        escaped++;
        text++;
    }
    if (!*text) {
        logParseError(text, "expected closing quote mark");
        return kParseError;
    }
    text++;

    char *scan = start + 1;
    char *result = (char *)Memory::allocate(escaped + 1);
    for (int i = 0; i < escaped; scan++, i++) {
        if (*scan == '\\') {
            scan++;
            switch (*scan) {
            /****/ case '"':  result[i] = '"';
            break; case '\\': result[i] = '\\';
            break; case 'n':  result[i] = '\n';
            break; case 'r':  result[i] = '\r';
            break; case 't':  result[i] = '\t';
            break;
            default:
                logParseError(scan, "unknown escape sequence");
                return kParseError;
            }
            continue;
        }
        result[i] = *scan;
    }
    result[escaped] = '\0';
    *contents = text;
    *out = result;
    return kParseOk;
}

bool Parser::consumeKeyword(char **contents, const char *keyword) {
    char *text = *contents;
    const char *compare = parseIdentifierAll(&text);
    if (!compare || strcmp(compare, keyword) != 0) {
        Memory::free((void *)compare);
        return false;
    }
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
    if (!gen) return;
    U_ASSERT(reference.m_key != NoSlot);
    Gen::addAssign(gen, reference.m_base, reference.m_key, value, kAssignPlain);
}

void Parser::Reference::assignExisting(Gen *gen, Reference reference, Slot value) {
    if (!gen) return;
    U_ASSERT(reference.m_key != NoSlot);
    Gen::addAssign(gen, reference.m_base, reference.m_key, value, kAssignExisting);
}

void Parser::Reference::assignShadowing(Gen *gen, Reference reference, Slot value) {
    if (!gen) return;
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
        FileRange *addEntryRange = Gen::newRange(text);
        char *keyName = (char *)parseIdentifier(&text);
        if (!keyName) {
            ParseResult result = parseString(&text, &keyName);
            if (result != kParseOk) {
                logParseError(text, "expected identifier");
                return kParseError;
            }
        }
        FileRange::recordEnd(text, addEntryRange);

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
            Gen::useRangeStart(gen, addEntryRange);
            Slot keySlot = Gen::addNewStringObject(gen, gen->m_scope, keyName);
            Slot valueSlot = Reference::access(gen, value);
            Gen::addAssign(gen, objectSlot, keySlot, valueSlot, kAssignPlain);
            Gen::useRangeEnd(gen, addEntryRange);
        }

        if (consumeString(&text, ","))
            continue;
        if (consumeString(&text, "}"))
            break;

        logParseError(text, "expected comma or closing brace");
        return kParseError;
    }
    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseObjectLiteral(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    FileRange *range = Gen::newRange(text);
    if (!consumeString(&text, "{")) {
        Gen::delRange(range);
        return kParseNone;
    }

    Slot objectSlot = 0;
    Gen::useRangeStart(gen, range);
    if (gen)
        objectSlot = Gen::addNewObject(gen, 0);
    Gen::useRangeEnd(gen, range);

    *contents = text;
    *reference = { objectSlot, Reference::NoSlot, Reference::kNone };

    ParseResult result = parseObjectLiteral(contents, gen, objectSlot);
    FileRange::recordEnd(text, range);
    return result;
}

ParseResult Parser::parseArrayLiteral(char **contents, Gen *gen, Slot objectSlot, FileRange *range) {
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

        logParseError(text, "expected comma or closing bracket");
        return kParseError;
    }

    *contents = text;
    if (gen) {
        Gen::useRangeStart(gen, range);
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
        Gen::useRangeEnd(gen, range);
    }

    return kParseOk;
}

ParseResult Parser::parseArrayLiteral(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    FileRange *literalRange = Gen::newRange(text);
    if (!consumeString(&text, "[")) {
        Gen::delRange(literalRange);
        return kParseNone;
    }
    Slot objectSlot = 0;
    Gen::useRangeStart(gen, literalRange);
    if (gen) {
        objectSlot = Gen::addNewArrayObject(gen, gen->m_scope);
    }
    *contents = text;
    *reference = { objectSlot, Reference::NoSlot, Reference::kNone };
    Gen::useRangeEnd(gen, literalRange);

    ParseResult result = parseArrayLiteral(contents, gen, objectSlot, literalRange);
    FileRange::recordEnd(text, literalRange);
    return result;
}

ParseResult Parser::parseExpressionStem(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    FileRange *range = Gen::newRange(text);
    const char *identifier = parseIdentifier(&text);

    if (identifier) {
        *contents = text;
        FileRange::recordEnd(text, range);
        Gen::useRangeStart(gen, range);
        *reference = Reference::getScope(gen, identifier);
        Gen::useRangeEnd(gen, range);
        return kParseOk;
    }

    // float?
    {
        float value = 0.0f;
        if (parseFloat(&text, &value)) {
            *contents = text;
            FileRange::recordEnd(text, range);
            Slot slot = 0;
            if (gen) {
                Gen::useRangeStart(gen, range);
                slot = Gen::addNewFloatObject(gen, gen->m_scope, value);
                Gen::useRangeEnd(gen, range);
            }
            *reference = { slot, Reference::NoSlot, Reference::kNone };
            return kParseOk;
        }
    }
    // int?
    {
        int value = 0;
        if (parseInteger(&text, &value)) {
            *contents = text;
            FileRange::recordEnd(text, range);
            Slot slot = 0;
            if (gen) {
                Gen::useRangeStart(gen, range);
                slot = Gen::addNewIntObject(gen, gen->m_scope, value);
                Gen::useRangeEnd(gen, range);
            }
            *reference = { slot, Reference::NoSlot, Reference::kNone };
            return kParseOk;
        }
    }
    // string?
    {
        ParseResult result;
        char *value = nullptr;
        if ((result = parseString(&text, &value)) != kParseNone) {
            Slot slot = 0;
            if (result == kParseOk) {
                *contents = text;
                FileRange::recordEnd(text, range);
                if (gen) {
                    Gen::useRangeStart(gen, range);
                    slot = Gen::addNewStringObject(gen, gen->m_scope, value);
                    Gen::useRangeEnd(gen, range);
                }
            }
            *reference = { slot, Reference::NoSlot, Reference::kNone };
            return result;
        }
    }

    ParseResult result;

    // object literal
    if ((result = parseObjectLiteral(&text, gen, reference)) != kParseNone) {
        if (result == kParseOk)
            *contents = text;
        Gen::delRange(range);
        return result;
    }

    // array literal
    if ((result = parseArrayLiteral(&text, gen, reference)) != kParseNone) {
        if (result == kParseOk)
            *contents = text;
        Gen::delRange(range);
        return result;
    }

    if (consumeString(&text, "(")) {
        Gen::delRange(range);

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
    if (consumeKeyword(&text, "fn")
    || (consumeKeyword(&text, "method") && (isMethod = true)))
    {
        FileRange::recordEnd(text, range);

        UserFunction *function;
        ParseResult result = parseFunctionExpression(&text, &function);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        *contents = text;

        Slot slot = 0;
        if (gen) {
            function->m_isMethod = isMethod;
            Gen::useRangeStart(gen, range); // count closure allocation as function
            slot = Gen::addNewClosureObject(gen, gen->m_scope, function);
            Gen::useRangeEnd(gen, range);
        }
        *reference = { slot, Reference::NoSlot, Reference::kNone };
        return kParseOk;
    }

    if (consumeKeyword(&text, "new")) {
        FileRange::recordEnd(text, range);

        Reference parentVariable;
        ParseResult result = parseExpression(&text, gen, 0, &parentVariable);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        Gen::useRangeStart(gen, range);
        Slot parentSlot = Reference::access(gen, parentVariable);
        Slot objectSlot = 0;
        if (gen)
            objectSlot = Gen::addNewObject(gen, parentSlot);
        Gen::useRangeEnd(gen, range);

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
    Gen::delRange(range);

    logParseError(text, "expected expression");
    return kParseError;
}

ParseResult Parser::parseCall(char **contents, Gen *gen, Reference *expression, FileRange *expressionRange) {
    char *text = *contents;
    FileRange *callRange = Gen::newRange(text);
    FileRange *exprRange = (FileRange *)Memory::allocate(sizeof *exprRange);
    if (gen)
        *exprRange = *expressionRange;

    if (!consumeString(&text, "(")) {
        Gen::delRange(callRange);
        Memory::free(exprRange);
        return kParseNone;
    }

    *contents = text;
    Slot *arguments = nullptr;
    size_t length = 0;

    while (!consumeString(&text, ")")) {
        if (length && !consumeString(&text, ",")) {
            logParseError(text, "expected comma or closing parenthesis");
            return kParseError;
        }

        Reference argument;
        ParseResult result = parseExpression(&text, gen, 0, &argument);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        Gen::useRangeStart(gen, callRange);
        Slot slot = Reference::access(gen, argument);
        Gen::useRangeEnd(gen, callRange);
        arguments = (Slot *)Memory::reallocate(arguments, sizeof(Slot) * ++length);
        arguments[length - 1] = slot;
    }

    FileRange::recordEnd(text, callRange);

    *contents = text;

    if (gen) {
        Slot thisSlot;
        if (expression->m_key)
            thisSlot = expression->m_base;
        else
            thisSlot = 0;

        Gen::useRangeStart(gen, exprRange);
        *expression = {
            Gen::addCall(gen, Reference::access(gen, *expression), thisSlot, arguments, length),
            Reference::NoSlot,
            Reference::kNone
        };
        Gen::useRangeEnd(gen, exprRange);
    }

    return kParseOk;
}

ParseResult Parser::parseArrayAccess(char **contents, Gen *gen, Reference *expression) {
    char *text = *contents;
    FileRange *accessRange = Gen::newRange(text);

    if (!consumeString(&text, "[")) {
        Gen::delRange(accessRange);
        return kParseNone;
    }

    Reference key;
    ParseResult result = parseExpression(&text, gen, 0, &key);
    if (result == kParseError)
        return result;

    U_ASSERT(result == kParseOk);

    if (!consumeString(&text, "]")) {
        logParseError(*contents, "expected closing ']'");
        return kParseError;
    }

    FileRange::recordEnd(text, accessRange);

    Gen::useRangeStart(gen, accessRange);
    Slot keySlot = Reference::access(gen, key);

    *contents = text;

    *expression = {
        Reference::access(gen, *expression),
        keySlot,
        Reference::kIndex
    };
    Gen::useRangeEnd(gen, accessRange);

    return kParseOk;
}

ParseResult Parser::parsePropertyAccess(char **contents, Gen *gen, Reference *expression) {
    char *text = *contents;

    FileRange *propertyRange = Gen::newRange(text);
    if (!consumeString(&text, ".")) {
        Gen::delRange(propertyRange);
        return kParseNone;
    }

    const char *keyName = parseIdentifier(&text);
    FileRange::recordEnd(text, propertyRange);
    *contents = text;

    Slot keySlot = 0;
    Gen::useRangeStart(gen, propertyRange);
    if (gen)
        keySlot = Gen::addNewStringObject(gen, gen->m_scope, keyName);
    *expression = {
        Reference::access(gen, *expression),
        keySlot,
        Reference::kObject
    };
    Gen::useRangeEnd(gen, propertyRange);

    return kParseOk;
}

ParseResult Parser::parseExpression(char **contents, Gen *gen, Reference *reference) {
    FileRange *exprRange = Gen::newRange(*contents);
    ParseResult result = parseExpressionStem(contents, gen, reference);
    if (result == kParseError) {
        Gen::delRange(exprRange);
        return result;
    }
    U_ASSERT(result == kParseOk);

    for (;;) {
        FileRange::recordEnd(*contents, exprRange);
        if ((result = parseCall(contents, gen, reference, exprRange)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        if ((result = parsePropertyAccess(contents, gen, reference)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        if ((result = parseArrayAccess(contents, gen, reference)) == kParseOk)
            continue;
        if ((result = parsePostfix(contents, gen, reference)) == kParseOk)
            continue;
        if (result == kParseError)
            return result;
        break;
    }

    return kParseOk;
}

void Parser::buildOperation(Gen *gen, const char *op, Reference *result, Reference lhs, Reference rhs, FileRange *range)
{
    if (!gen) return;
    Gen::useRangeStart(gen, range);
    Slot lhsSlot = Reference::access(gen, lhs);
    Slot rhsSlot = Reference::access(gen, rhs);
    Slot function = Gen::addAccess(gen, lhsSlot, Gen::addNewStringObject(gen, gen->m_scope, op));
    *result = { Gen::addCall(gen, function, lhsSlot, rhsSlot), Reference::NoSlot, Reference::kNone };
    Gen::useRangeEnd(gen, range);
}

bool Parser::assignSlot(Gen *gen, Reference ref, Slot value, FileRange *assignRange) {
    switch (ref.m_mode) {
    case Reference::kNone:
        Gen::useRangeEnd(gen, assignRange);
        Gen::delRange(assignRange);
        return false;
    case Reference::kVariable:
        Reference::assignExisting(gen, ref, value);
        break;
    case Reference::kObject:
        Reference::assignShadowing(gen, ref, value);
        break;
    case Reference::kIndex:
        Reference::assignPlain(gen, ref, value);
        break;
    default:
        U_ASSERT(0 && "internal compiler error");
        return false;
    }
    Gen::useRangeEnd(gen, assignRange);
    return true;
}

ParseResult Parser::parsePostfix(char **contents, Gen *gen, Reference *reference) {
    char *text = *contents;
    FileRange *operatorRange = Gen::newRange(text);
    const char *op = nullptr;
    if (consumeString(&text, "++")) {
        op = "+";
    } else if (consumeString(&text, "--")) {
        op = "-";
    } else {
        Gen::delRange(operatorRange);
        return kParseNone;
    }

    FileRange::recordEnd(text, operatorRange);

    Gen::useRangeStart(gen, operatorRange);
    Slot prev = 0;
    Slot one = 0;
    if (gen) {
        prev = Reference::access(gen, *reference);
        one = Gen::addNewIntObject(gen, gen->m_scope, 1);
    }
    Gen::useRangeEnd(gen, operatorRange);

    Reference sum;
    buildOperation(gen, op, &sum, *reference,
        { one, Reference::NoSlot, Reference::kNone }, operatorRange);
    Gen::useRangeStart(gen, operatorRange);
    if (!assignSlot(gen, *reference, Reference::access(gen, sum), operatorRange)) {
        logParseError(*contents, "postfix cannot assign: expression is non-reference");
        return kParseError;
    }
    *contents = text;
    *reference = { prev, Reference::NoSlot, Reference::kNone };

    return kParseOk;
}

ParseResult Parser::parseExpression(char **contents, Gen *gen, int level, Reference *reference) {
    char *text = *contents;

    bool negate = false;
    FileRange *negatedRange = Gen::newRange(text);
    if (consumeString(&text, "-")) {
        FileRange::recordEnd(text, negatedRange);
        negate = true;
    } else {
        Gen::delRange(negatedRange);
    }

    ParseResult result = parseExpression(&text, gen, reference);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    if (negate) {
        Gen::useRangeStart(gen, negatedRange);
        Slot zero = 0;
        if (gen) {
            zero = Gen::addNewIntObject(gen, gen->m_scope, 0);
        }
        Gen::useRangeEnd(gen, negatedRange);
        Reference ref = { zero, Reference::NoSlot, Reference::kNone };
        buildOperation(gen, "-", reference, ref, *reference, negatedRange);
    }

    Reference expression;

    // Precedence level 4:
    if (level > 4) {
        *contents = text;
        return kParseOk;
    }

    for (;;) {
        FileRange *range = Gen::newRange(text);
        if (consumeString(&text, "&")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 4, &expression);
            if (result == kParseError)
                return kParseError;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "&", reference, *reference, expression, range);
            continue;
        }
        Gen::delRange(range);
        break;
    }

    // Precedence level 3:
    if (level > 3) {
        *contents = text;
        return kParseOk;
    }

    for (;;) {
        FileRange *range = Gen::newRange(text);
        if (consumeString(&text, "|")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 4, &expression);
            if (result == kParseError)
                return kParseError;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "|", reference, *reference, expression, range);
            continue;
        }
        Gen::delRange(range);
        break;
    }

    // Precedence level 2:
    if (level > 2) {
        *contents = text;
        return kParseOk;
    }

    for (;;) {
        FileRange *range = Gen::newRange(text);
        if (consumeString(&text, "*")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 3, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "*", reference, *reference, expression, range);
            continue;
        }
        if (consumeString(&text, "/")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 3, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "/", reference, *reference, expression, range);
            continue;
        }
        Gen::delRange(range);
        break;
    }

    // Precedence level 1:
    if (level > 1) {
        *contents = text;
        return kParseOk;
    }

    for (;;) {
        FileRange *range = Gen::newRange(text);
        if (consumeString(&text, "+")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 2, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "+", reference, *reference, expression, range);
            continue;
        }
        if (consumeString(&text, "-")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 2, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "-", reference, *reference, expression, range);
            continue;
        }
        Gen::delRange(range);
        break;
    }

    // Precedence level 0:

    if (level > 0) {
        *contents = text;
        return kParseOk;
    }

    bool negated = false;

    FileRange *range = Gen::newRange(text);
    if (consumeString(&text, "==")) {
        FileRange::recordEnd(text, range);
        result = parseExpression(&text, gen, 1, &expression);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
        buildOperation(gen, "==", reference, *reference, expression, range);
    } else if (consumeString(&text, "!=")) {
        FileRange::recordEnd(text, range);
        // the same code except result is negated
        result = parseExpression(&text, gen, 1, &expression);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
        buildOperation(gen, "==", reference, *reference, expression, range);
        negated = true;
    } else {
        // this allows for operators like !<, !>, !<=, !>=
        if (consumeString(&text, "!"))
            negated = true;
        if (consumeString(&text, "<=")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "<=", reference, *reference, expression, range);
        } else if (consumeString(&text, ">=")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, ">=", reference, *reference, expression, range);
        } else if (consumeString(&text, "<")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, "<", reference, *reference, expression, range);
        } else if (consumeString(&text, ">")) {
            FileRange::recordEnd(text, range);
            result = parseExpression(&text, gen, 1, &expression);
            if (result == kParseError)
                return result;
            U_ASSERT(result == kParseOk);
            buildOperation(gen, ">", reference, *reference, expression, range);
        } else if (negated) {
            logParseError(text, "expected comparison operator");
            return kParseError;
        }
    }

    if (negated && gen) {
        Gen::useRangeStart(gen, range);
        Slot lhs = Reference::access(gen, *reference);
        Slot bnot = Gen::addAccess(gen, lhs, Gen::addNewStringObject(gen, gen->m_scope, "!"));
        *reference = { Gen::addCall(gen, bnot, lhs), Reference::NoSlot, Reference::kNone };
        Gen::useRangeEnd(gen, range);
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseIfStatement(char **contents, Gen *gen, FileRange *keywordRange) {
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

    Gen::useRangeStart(gen, keywordRange);
    Slot testSlot = Reference::access(gen, testExpression);
    if (!consumeString(&text, ")")) {
        logParseError(text, "expected closing paren after 'if'");
        return kParseError;
    }
    BlockRef trueBlock;
    BlockRef falseBlock;
    BlockRef endBlock;
    Gen::addTestBranch(gen, testSlot, &trueBlock, &falseBlock);
    Gen::useRangeEnd(gen, keywordRange);

    Gen::setBlockRef(gen, trueBlock, Gen::newBlock(gen));

    result = parseBlock(&text, gen);
    if (result == kParseError)
        return kParseError;
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(gen, keywordRange);
    Gen::addBranch(gen, &endBlock);
    Gen::useRangeEnd(gen, keywordRange);

    size_t falseBlockIndex = Gen::newBlock(gen);
    Gen::setBlockRef(gen, falseBlock, falseBlockIndex);
    if (consumeKeyword(&text, "else")) {
        result = parseBlock(&text, gen);
        if (result == kParseError)
            return kParseError;
        U_ASSERT(result == kParseOk);
        BlockRef elseEndBlock;
        Gen::useRangeStart(gen, keywordRange);
        Gen::addBranch(gen, &elseEndBlock);
        Gen::useRangeEnd(gen, keywordRange);

        size_t blockIndex = Gen::newBlock(gen);
        Gen::setBlockRef(gen, elseEndBlock, blockIndex);
        Gen::setBlockRef(gen, endBlock, blockIndex);
    } else {
        Gen::setBlockRef(gen, endBlock, falseBlockIndex);
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseWhile(char **contents, Gen *gen, FileRange *range) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening paren after 'while'");
        return kParseError;
    }

    Gen::useRangeStart(gen, range);
    BlockRef testBlock;
    Gen::addBranch(gen, &testBlock);
    size_t testBlockIndex = Gen::newBlock(gen);
    Gen::setBlockRef(gen, testBlock, testBlockIndex);
    Gen::useRangeEnd(gen, range);

    BlockRef loopBlock;
    BlockRef endBlock;

    Reference testExpression;
    ParseResult result = parseExpression(&text, gen, 0, &testExpression);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(gen, range);
    Slot testSlot = Reference::access(gen, testExpression);
    if (!consumeString(&text, ")")) {
        logParseError(text, "expected closing paren after 'while'");
        return kParseError;
    }
    Gen::addTestBranch(gen, testSlot, &loopBlock, &endBlock);
    Gen::useRangeEnd(gen, range);

    Gen::setBlockRef(gen, loopBlock, Gen::newBlock(gen));
    result = parseBlock(&text, gen);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(gen, range);
    BlockRef testBlock2;
    Gen::addBranch(gen, &testBlock2);
    Gen::useRangeEnd(gen, range);

    Gen::setBlockRef(gen, testBlock2, testBlockIndex);
    Gen::setBlockRef(gen, endBlock, Gen::newBlock(gen));

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseLetDeclaration(char **contents, Gen *gen, FileRange *letRange, bool isConst) {
    char *text = *contents;

    // allocate a new scope immediately to allow recursion for closures
    // i.e allow: let foo = fn() { foo(); };
    Gen::useRangeStart(gen, letRange);
    gen->m_scope = Gen::addNewObject(gen, gen->m_scope);
    const size_t letScope = gen->m_scope;
    Gen::useRangeEnd(gen, letRange);

    FileRange *letName = Gen::newRange(text);
    const char *variableName = parseIdentifier(&text);
    FileRange::recordEnd(text, letName);

    Gen::useRangeStart(gen, letName);
    Slot value;
    Slot variableNameSlot = Gen::addNewStringObject(gen, letScope, variableName);
    Gen::addAssign(gen, letScope, variableNameSlot, 0, kAssignPlain);
    Gen::addCloseObject(gen, letScope);
    Gen::useRangeEnd(gen, letName);

    FileRange *assignRange = Gen::newRange(text);

    if (!consumeString(&text, "=")) {
        Gen::delRange(assignRange);
        assignRange = letName;
        value = 0;
    } else {
        FileRange::recordEnd(text, assignRange);
        Reference reference;

        FileRange *expressionRange = Gen::newRange(text);
        ParseResult result = parseExpression(&text, gen, 0, &reference);
        FileRange::recordEnd(text, expressionRange);

        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);

        Gen::useRangeStart(gen, expressionRange);
        value = Reference::access(gen, reference);
        Gen::useRangeEnd(gen, expressionRange);
    }

    Gen::useRangeStart(gen, assignRange);
    Gen::addAssign(gen, letScope, variableNameSlot, value, kAssignExisting);
    if (isConst) {
        Gen::addFreeze(gen, letScope);
    }
    Gen::useRangeEnd(gen, assignRange);

    // let a, b
    if (consumeString(&text, ",")) {
        *contents = text;
        return parseLetDeclaration(contents, gen, letRange, isConst);
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseAssign(char **contents, Gen *gen) {
    char *text = *contents;
    Reference ref;
    ParseResult result = parseExpression(&text, nullptr, &ref); // speculative parse
    if (result != kParseOk)
        return result;

    const char *operation = nullptr;
    const char *assignment = nullptr;
    if (consumeString(&text, "+=")) {
        operation = "+";
        assignment = "+=";
    } else if (consumeString(&text, "-=")) {
        operation = "-";
        assignment = "-=";
    } else if (consumeString(&text, "*=")) {
        operation = "*";
        assignment = "*=";
    } else if (consumeString(&text, "/=")) {
        operation = "/";
        assignment = "/=";
    } else if (consumeString(&text, "=")) {
        assignment = "=";
    } else {
        return kParseNone;
    }

    text = *contents;
    result = parseExpression(&text, gen, &ref);
    U_ASSERT(result == kParseOk);
    FileRange *assignRange = Gen::newRange(text);
    if (!consumeString(&text, assignment))
        U_ASSERT(0 && "internal compiler error");
    FileRange::recordEnd(text, assignRange);

    Reference valueExpression;
    result = parseExpression(&text, gen, 0, &valueExpression);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    if (operation) {
        buildOperation(gen,
                       operation,
                       &valueExpression,
                       ref,
                       valueExpression,
                       assignRange);
    }

    Gen::useRangeStart(gen, assignRange);
    Slot value = Reference::access(gen, valueExpression);

    if (!assignSlot(gen, ref, value, assignRange)) {
        logParseError(text, "cannot perform assignment: expression is non-reference");
        return kParseError;
    }

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseForStatement(char **contents, Gen *gen, FileRange *range) {
    char *text = *contents;
    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening parenthesis in 'for'");
        return kParseError;
    }

    // variable is out of scope after the for loop
    size_t backup = Gen::scopeEnter(gen);

    FileRange *declarationRange = Gen::newRange(text);
    if (consumeKeyword(&text, "let")) {
        ParseResult result = parseLetDeclaration(&text, gen, declarationRange, false);
        if (result == kParseError)
            return kParseError;
        if (result == kParseNone) {
            logParseError(text, "expected let declaration or assignment in 'for'");
            return kParseError;
        }
    } else {
        ParseResult result = parseAssign(&text, gen);
        if (result == kParseError)
            return kParseError;
        U_ASSERT(result == kParseOk);
    }

    if (!consumeString(&text, ";")) {
        logParseError(text, "expected semicolon in 'for'");
        return kParseError;
    }

    Gen::useRangeStart(gen, range);
    BlockRef testBlock;
    Gen::addBranch(gen, &testBlock);
    size_t testBlockIndex = Gen::newBlock(gen);
    Gen::setBlockRef(gen, testBlock, testBlockIndex);
    Gen::useRangeEnd(gen, range);

    BlockRef loopBlock;
    BlockRef endBlock;

    Reference testExpression;
    ParseResult result = parseExpression(&text, gen, 0, &testExpression);
    if (result == kParseError)
        return kParseError;
    U_ASSERT(result == kParseOk);

    if (!consumeString(&text, ";")) {
        logParseError(text, "expected semicolon in 'for'");
        return kParseError;
    }

    char *step = text;
    {
        result = parseSemicolonStatement(&text, nullptr);
        if (result == kParseError)
            return kParseError;
        if (result == kParseNone) {
            logParseError(text, "expected assignment in 'for'");
            return kParseError;
        }
        U_ASSERT(result == kParseOk);
        if (!consumeString(&text, ")")) {
            logParseError(text, "expected closing parenthesis in 'for'");
            return kParseError;
        }
    }

    Gen::useRangeStart(gen, range);
    Slot testSlot = Reference::access(gen, testExpression);

    Gen::addTestBranch(gen, testSlot, &loopBlock, &endBlock);
    Gen::useRangeEnd(gen, range);

    // loop body
    Gen::setBlockRef(gen, loopBlock, Gen::newBlock(gen));

    parseBlock(&text, gen);

    result = parseSemicolonStatement(&step, gen);
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(gen, range);
    BlockRef terminateBlock;
    Gen::addBranch(gen, &terminateBlock);
    Gen::setBlockRef(gen, terminateBlock, testBlockIndex);

    Gen::useRangeEnd(gen, range);

    Gen::setBlockRef(gen, endBlock, Gen::newBlock(gen));

    Gen::scopeLeave(gen, backup);

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseReturnStatement(char **contents, Gen *gen, FileRange *keywordRange) {
    Reference returnValue;
    ParseResult result = parseExpression(contents, gen, 0, &returnValue);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(gen, keywordRange);
    Slot value = Reference::access(gen, returnValue);

    Gen::addReturn(gen, value);
    Gen::newBlock(gen);
    Gen::useRangeEnd(gen, keywordRange);

    return kParseOk;
}

ParseResult Parser::parseFunctionDeclaration(char **contents, Gen *gen, FileRange *range) {
    Gen::useRangeStart(gen, range);
    gen->m_scope = Gen::addNewObject(gen, gen->m_scope);
    Gen::useRangeEnd(gen, range);

    UserFunction *function;
    ParseResult result = parseFunctionExpression(contents, &function);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);
    Gen::useRangeStart(gen, range);
    Slot nameSlot = Gen::addNewStringObject(gen, gen->m_scope, function->m_name);
    Slot slot = Gen::addNewClosureObject(gen, gen->m_scope, function);
    Gen::addAssign(gen, gen->m_scope, nameSlot, slot, kAssignPlain);
    Gen::addCloseObject(gen, gen->m_scope);
    Gen::addFreeze(gen, gen->m_scope);
    Gen::useRangeEnd(gen, range);
    return kParseOk;
}

ParseResult Parser::parseSemicolonStatement(char **contents, Gen *gen) {
    char *text = *contents;
    FileRange *keywordRange = Gen::newRange(text);
    ParseResult result;
    if (consumeKeyword(&text, "return")) {
        FileRange::recordEnd(text, keywordRange);
        result = parseReturnStatement(&text, gen, keywordRange);
    } else if (consumeKeyword(&text, "let")) {
        FileRange::recordEnd(text, keywordRange);
        result = parseLetDeclaration(&text, gen, keywordRange, false);
    } else if (consumeKeyword(&text, "const")) {
        FileRange::recordEnd(text, keywordRange);
        result = parseLetDeclaration(&text, gen, keywordRange, true);
    } else {
        result = parseAssign(&text, gen);
        if (result == kParseNone) {
            // expressions as statements
            Reference ref;
            result = parseExpression(&text, gen, &ref);
            if (result == kParseNone) {
                Gen::delRange(keywordRange);
                return kParseNone;
            }
        }
    }

    if (result == kParseError)
        return kParseError;
    U_ASSERT(result == kParseOk);

    *contents = text;
    return kParseOk;
}

ParseResult Parser::parseStatement(char **contents, Gen *gen) {
    char *text = *contents;
    FileRange *keywordRange = Gen::newRange(text);
    if (consumeKeyword(&text, "if")) {
        FileRange::recordEnd(text, keywordRange);
        *contents = text;
        return parseIfStatement(contents, gen, keywordRange);
    }
    if (consumeKeyword(&text, "fn")) {
        FileRange::recordEnd(text, keywordRange);
        *contents = text;
        return parseFunctionDeclaration(contents, gen, keywordRange);
    }
    if (consumeKeyword(&text, "while")) {
        FileRange::recordEnd(text, keywordRange);
        *contents = text;
        return parseWhile(contents, gen, keywordRange);
    }
    if (consumeKeyword(&text, "for")) {
        FileRange::recordEnd(text, keywordRange);
        *contents = text;
        return parseForStatement(contents, gen, keywordRange);
    }

    // expression as statement
    ParseResult result = parseSemicolonStatement(&text, gen);
    if (result == kParseError)
        return kParseError;
    if (result == kParseOk) {
        if (!consumeString(&text, ";")) {
            logParseError(text, "expected ';' after statement");
            return kParseError;
        }
        if (result == kParseOk)
            *contents = text;
        return result;
    }

    logParseError(text, "unknown statement");
    return kParseError;
}

ParseResult Parser::parseBlock(char **contents, Gen *gen) {
    char *text = *contents;

    // Note: blocks don't actually open new scopes
    size_t backup = Gen::scopeEnter(gen);

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
    Gen::scopeLeave(gen, backup);
    return kParseOk;
}

ParseResult Parser::parseFunctionExpression(char **contents, UserFunction **function) {
    char *text = *contents;
    const char *functionName = parseIdentifier(&text);

    FileRange *functionFrameRange = Gen::newRange(text);
    if (!consumeString(&text, "(")) {
        logParseError(text, "expected opening paren for parameter list");
        return kParseError;
    }

    u::vector<const char *> arguments;
    while (!consumeString(&text, ")")) {
        if (arguments.size() && !consumeString(&text, ",")) {
            logParseError(text, "expected comma in parameter list");
            return kParseError;
        }
        const char *argument = parseIdentifier(&text);
        if (!argument) {
            logParseError(text, "expected identifier for parameter in parameter list");
            return kParseError;
        }
        arguments.push_back(argument);
    }
    FileRange::recordEnd(text, functionFrameRange);

    *contents = text;

    Gen gen = { };
    gen.m_count = arguments.size();
    gen.m_slot = arguments.size() + 1;
    gen.m_name = functionName;
    gen.m_blockTerminated = true;

    // generate lexical scope
    Gen::newBlock(&gen);
    Gen::useRangeStart(&gen, functionFrameRange);
    Slot contextSlot = Gen::addGetContext(&gen);
    gen.m_scope = Gen::addNewObject(&gen, contextSlot);
    for (size_t i = 0; i < arguments.size(); i++) {
        Slot argumentSlot = Gen::addNewStringObject(&gen, gen.m_scope, arguments[i]);
        Gen::addAssign(&gen, gen.m_scope, argumentSlot, i + 1, kAssignPlain);
    }
    Gen::addCloseObject(&gen, gen.m_scope);
    Gen::useRangeEnd(&gen, functionFrameRange);

    ParseResult result = parseBlock(contents, &gen);
    if (result == kParseError)
        return result;
    U_ASSERT(result == kParseOk);

    Gen::useRangeStart(&gen, functionFrameRange);
    Gen::terminate(&gen);
    Gen::useRangeEnd(&gen, functionFrameRange);

    *function = Gen::optimize(Gen::buildFunction(&gen));
    return kParseOk;
}

ParseResult Parser::parseModule(char **contents, UserFunction **function) {
    Gen gen = { };
    gen.m_blockTerminated = true;
    gen.m_slot = 1;

    // capture future module statement
    FileRange *moduleRange = Gen::newRange(*contents);
    FileRange::recordEnd(*contents, moduleRange);

    Gen::useRangeStart(&gen, moduleRange);
    Gen::newBlock(&gen);
    gen.m_scope = Gen::addGetContext(&gen);
    Gen::useRangeEnd(&gen, moduleRange);

    for (;;) {
        consumeFiller(contents);
        if ((*contents)[0] == '\0')
            break;
        ParseResult result = parseStatement(contents, &gen);
        if (result == kParseError)
            return result;
        U_ASSERT(result == kParseOk);
    }

    Gen::useRangeStart(&gen, moduleRange);
    Gen::addReturn(&gen, gen.m_scope);
    Gen::useRangeEnd(&gen, moduleRange);
    *function = Gen::optimize(Gen::buildFunction(&gen));
    return kParseOk;
}

}
