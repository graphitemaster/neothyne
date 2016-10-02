#ifndef S_PARSER_HDR
#define S_PARSER_HDR

#include "s_instr.h"

namespace s {

struct Gen;
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

        static Reference getScope(Gen *gen, const char *name);
        static Slot access(Gen *gen, Reference reference);
        static void assignPlain(Gen *gen, Reference reference, Slot value);
        static void assignExisting(Gen *gen, Reference reference, Slot value);
        static void assignShadowing(Gen *gen, Reference reference, Slot value);

        Slot m_base;
        Slot m_key;
        Mode m_mode;
    };

    static const char *parseIdentifier(char **contents);
    static const char *parseIdentifierAll(char **contents);

    static bool parseInteger(char **contents, int *out);
    static bool parseFloat(char **contents, float *out);
    static bool parseString(char **contents, char **out);

    static Reference parseExpression(char **contents, Gen *gen, int level);
    static Reference parseExpression(char **contents, Gen *gen);
    static Reference parseExpressionStem(char **contents, Gen *gen);

    static bool parseCall(char **contents, Gen *gen, Reference *expression);
    static bool parseObjectLiteral(char **contents, Gen *gen, Reference *reference);
    static void parseObjectLiteral(char **contents, Gen *gen, Slot objectSlot);
    static void parseArrayLiteral(char **contents, Gen *gen, Slot objectSlot);
    static bool parseArrayLiteral(char **contents, Gen *gen, Reference *reference);
    static bool parsePropertyAccess(char **contents, Gen *gen, Reference *expression);
    static bool parseArrayAccess(char **contents, Gen *gen, Reference *expression);

    static void parseBlock(char **contents, Gen *gen);
    static void parseIfStatement(char **contents, Gen *gen);
    static void parseWhile(char **contents, Gen *gen);
    static void parseReturnStatement(char **contents, Gen *gen);
    static void parseLetDeclaration(char **contents, Gen *gen);
    static void parseFunctionDeclaration(char **contents, Gen *gen);
    static void parseStatement(char **contents, Gen *gen);

    static UserFunction *parseFunctionExpression(char **contents);
    static UserFunction *parseModule(char **contents);
};

}

#endif
