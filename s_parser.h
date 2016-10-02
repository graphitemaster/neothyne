#ifndef S_PARSER_HDR
#define S_PARSER_HDR

#include "s_instr.h"

namespace s {

struct Gen;
struct UserFunction;

enum ParseResult {
    kParseNone,
    kParseError,
    kParseOk
};

struct Parser {
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

    static void consumeFiller(char **contents);
    static bool consumeString(char **contents, const char *identifier);
    static bool consumeKeyword(char **contents, const char *keyword);

    static const char *parseIdentifierAll(char **contents);
    static const char *parseIdentifier(char **contents);

    static bool parseInteger(char **contents, int *out);
    static bool parseFloat(char **contents, float *out);
    static ParseResult parseString(char **contents, char **out);

    static ParseResult parseObjectLiteral(char **contents, Gen *gen, Slot objectSlot);
    static ParseResult parseObjectLiteral(char **contents, Gen *gen, Reference *reference);
    static ParseResult parseArrayLiteral(char **contents, Gen *gen, Slot objectSlot);
    static ParseResult parseArrayLiteral(char **contents, Gen *gen, Reference *reference);
    static ParseResult parseExpressionStem(char **contents, Gen *gen, Reference *reference);
    static ParseResult parseCall(char **contents, Gen *gen, Reference *expression);
    static ParseResult parseArrayAccess(char **contents, Gen *gen, Reference *expression);
    static ParseResult parsePropertyAccess(char **contents, Gen *gen, Reference *expression);
    static ParseResult parseExpression(char **contents, Gen *gen, Reference *reference);
    static ParseResult parseExpression(char **contents, Gen *gen, int level, Reference *reference);
    static ParseResult parseIfStatement(char **contents, Gen *gen);
    static ParseResult parseWhile(char **contents, Gen *gen);
    static ParseResult parseReturnStatement(char **contents, Gen *gen);
    static ParseResult parseLetDeclaration(char **contents, Gen *gen);
    static ParseResult parseFunctionDeclaration(char **contents, Gen *gen);
    static ParseResult parseStatement(char **contents, Gen *gen);
    static ParseResult parseBlock(char **contents, Gen *gen);
    static ParseResult parseFunctionExpression(char **contents, UserFunction **function);
    static ParseResult parseModule(char **contents, UserFunction **function);
};

}

#endif
