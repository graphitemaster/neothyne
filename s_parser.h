#ifndef S_PARSER_HDR
#define S_PARSER_HDR

#include "s_instr.h"
#include "s_util.h"

namespace s {

struct Gen;
struct UserFunction;
struct Memory;

enum ParseResult {
    kParseNone,
    kParseError,
    kParseOk
};

struct Parser {
    static ParseResult parseModule(Memory *memory, char **contents, UserFunction **function);

private:
    friend struct FileRange;
    friend struct SourceRange;

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
    static bool consumeKeyword(Memory *memory, char **contents, const char *keyword);

    static const char *parseIdentifierAll(Memory *memory, char **contents);
    static const char *parseIdentifier(Memory *memory, char **contents);

    static bool parseInteger(Memory *memory, char **contents, int *out);
    static bool parseFloat(Memory *memory, char **contents, float *out);
    static ParseResult parseString(Memory *memory, char **contents, char **out);

    static ParseResult parseAssign(Memory *memory, char **contents, Gen *gen);
    static ParseResult parseSemicolonStatement(Memory *memory, char **contents, Gen *gen);
    static ParseResult parseForStatement(Memory *memory, char **contents, Gen *gen, FileRange *range);
    static ParseResult parseObjectLiteral(Memory *memory, char **contents, Gen *gen, Slot objectSlot);
    static ParseResult parseObjectLiteral(Memory *memory, char **contents, Gen *gen, Reference *reference);
    static ParseResult parseArrayLiteral(Memory *memory, char **contents, Gen *gen, Slot objectSlot, FileRange *range);
    static ParseResult parseArrayLiteral(Memory *memory, char **contents, Gen *gen, Reference *reference);
    static ParseResult parseExpressionStem(Memory *memory, char **contents, Gen *gen, Reference *reference);
    static ParseResult parseCall(Memory *memory, char **contents, Gen *gen, Reference *expression, FileRange *expressionRange);
    static ParseResult parseArrayAccess(Memory *memory, char **contents, Gen *gen, Reference *expression);
    static ParseResult parsePropertyAccess(Memory *memory, char **contents, Gen *gen, Reference *expression);
    static ParseResult parseExpression(Memory *memory, char **contents, Gen *gen, Reference *reference);
    static ParseResult parseExpression(Memory *memory, char **contents, Gen *gen, int level, Reference *reference);
    static ParseResult parseIfStatement(Memory *memory, char **contents, Gen *gen, FileRange *keywordRange);
    static ParseResult parseWhile(Memory *memory, char **contents, Gen *gen, FileRange *range);
    static ParseResult parseReturnStatement(Memory *memory, char **contents, Gen *gen, FileRange *range);
    static ParseResult parseLetDeclaration(Memory *memory, char **contents, Gen *gen, FileRange *range, bool isConst);
    static ParseResult parseFunctionDeclaration(Memory *memory, char **contents, Gen *gen, FileRange *range);
    static ParseResult parseStatement(Memory *memory, char **contents, Gen *gen);
    static ParseResult parseBlock(Memory *memory, char **contents, Gen *gen);
    static ParseResult parseFunctionExpression(Memory *memory, char **contents, Gen *gen, UserFunction **function);
    static ParseResult parsePostfix(char **contents, Gen *gen, Reference *reference);

    static bool assignSlot(Gen *gen, Reference ref, Slot slot, FileRange *assignRange);
    static void buildOperation(Gen *gen, const char *op, Reference *result, Reference lhs, Reference rhs, FileRange *range);
};

}

#endif
