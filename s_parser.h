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

    static const char *parseIdentifier(char **contents);
    static const char *parseIdentifierAll(char **contents);

    static bool parseInteger(char **contents, int *out);
    static Slot parseExpression(char **contents, FunctionCodegen *generator, int level);
    static Slot parseExpression(char **contents, FunctionCodegen *generator);
    static Slot parseExpressionTail(char **contents, FunctionCodegen *generator);
    static bool parseCall(char **contents, FunctionCodegen *generator, Slot *expression);

    static void parseBlock(char **contents, FunctionCodegen *generator);
    static void parseIfStatement(char **contents, FunctionCodegen *generator);
    static void parseReturnStatement(char **contents, FunctionCodegen *generator);
    static void parseLetDeclaration(char **contents, FunctionCodegen *generator);
    static void parseStatement(char **contents, FunctionCodegen *generator);

    static UserFunction *parseFunctionExpression(char **contents);
    static UserFunction *parseModule(char **contents);
};

}

#endif
