#ifndef S_PARSER_HDR
#define S_PARSER_HDR

#include "s_instr.h"

namespace s {

struct FunctionCodegen;
struct UserFunction;

struct Parser {
    // Lexer
    static void consumeWhitespace(char **contents);
    static void consumeFiller(char **contents);
    static bool consumeString(char **contents, const char *identifier);
    static bool consumeKeyword(char **contents, const char *keyword);

    // reference to a Slot
    // generates accesses and assignments when working with a Slot
    struct Reference {
        Reference(Slot base, const char *key, bool isVariable);
        Slot access(FunctionCodegen *generator);
        void assign(FunctionCodegen *generator, Slot value);
        void assignExisting(FunctionCodegen *generator, Slot value);
        static Reference getScope(FunctionCodegen *generator, const char *name);
        bool isVariable() const { return m_isVariable; }
    private:
        Slot m_base;
        const char *m_key;
        bool m_isVariable;
    };

    static const char *parseIdentifier(char **contents);
    static const char *parseIdentifierAll(char **contents);

    static bool parseInteger(char **contents, int *out);
    static bool parseFloat(char **contents, float *out);
    static bool parseString(char **contents, char **out);

    static Reference parseExpression(char **contents, FunctionCodegen *generator, int level);
    static Reference parseExpression(char **contents, FunctionCodegen *generator);
    static Reference parseExpressionStem(char **contents, FunctionCodegen *generator);

    static bool parseCall(char **contents, FunctionCodegen *generator, Reference *expression);
    static bool parseObjectLiteral(char **contents, FunctionCodegen *generator, Reference *reference);
    static bool parsePropertyAccess(char **contents, FunctionCodegen *generator, Reference *expression);

    static void parseBlock(char **contents, FunctionCodegen *generator);
    static void parseIfStatement(char **contents, FunctionCodegen *generator);
    static void parseReturnStatement(char **contents, FunctionCodegen *generator);
    static void parseLetDeclaration(char **contents, FunctionCodegen *generator);
    static void parseFunctionDeclaration(char **contents, FunctionCodegen *generator);
    static void parseStatement(char **contents, FunctionCodegen *generator);

    static UserFunction *parseFunctionExpression(char **contents);
    static UserFunction *parseModule(char **contents);
};

}

#endif
