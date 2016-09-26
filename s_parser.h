#ifndef S_PARSER_HDR
#define S_PARSER_HDR

#include "s_instr.h"

namespace s {

struct Generator;
struct UserFunction;

struct Parser {
    // Lexer
    static void consumeFiller(char **contents);
    static bool consumeString(char **contents, const char *identifier);
    static bool consumeKeyword(char **contents, const char *keyword);

    struct Reference {
        static constexpr Slot NoSlot = (Slot)-1;

        enum Mode {
            kNone = -1, // not a reference
            kVariable,  // overwrite the key if found, otherwise error
            kObject,    // shadow the key if found, otherwise error
            kIndex      // write the key
        };

        static Reference getScope(Generator *generator, const char *name);
        static Slot access(Generator *generator, Reference reference);
        static void assignPlain(Generator *generator, Reference reference, Slot value);
        static void assignExisting(Generator *generator, Reference reference, Slot value);
        static void assignShadowing(Generator *generator, Reference reference, Slot value);

        Slot m_base;
        Slot m_key;
        Mode m_mode;
    };

    static const char *parseIdentifier(char **contents);
    static const char *parseIdentifierAll(char **contents);

    static bool parseInteger(char **contents, int *out);
    static bool parseFloat(char **contents, float *out);
    static bool parseString(char **contents, char **out);

    static Reference parseExpression(char **contents, Generator *generator, int level);
    static Reference parseExpression(char **contents, Generator *generator);
    static Reference parseExpressionStem(char **contents, Generator *generator);

    static bool parseCall(char **contents, Generator *generator, Reference *expression);
    static bool parseObjectLiteral(char **contents, Generator *generator, Reference *reference);
    static void parseObjectLiteral(char **contents, Generator *generator, Slot objectSlot);
    static void parseArrayLiteral(char **contents, Generator *generator, Slot objectSlot);
    static bool parseArrayLiteral(char **contents, Generator *generator, Reference *reference);
    static bool parsePropertyAccess(char **contents, Generator *generator, Reference *expression);
    static bool parseArrayAccess(char **contents, Generator *generator, Reference *expression);

    static void parseBlock(char **contents, Generator *generator);
    static void parseIfStatement(char **contents, Generator *generator);
    static void parseWhile(char **contents, Generator *generator);
    static void parseReturnStatement(char **contents, Generator *generator);
    static void parseLetDeclaration(char **contents, Generator *generator);
    static void parseFunctionDeclaration(char **contents, Generator *generator);
    static void parseStatement(char **contents, Generator *generator);

    static UserFunction *parseFunctionExpression(char **contents);
    static UserFunction *parseModule(char **contents);
};

}

#endif
