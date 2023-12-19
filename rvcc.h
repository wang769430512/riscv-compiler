#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

typedef enum
{
    TK_IDENT, // 标记符，可以为变量名、函数名
    TK_PUNCT,
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Token Token;
struct Token
{
    TokenKind Kind;
    Token *Next;
    int Val;
    char *Loc;
    int Len;
};
Token *tokenize(char *Input);

void error(char *Fmt, ...);
//void verrorAt(char *Loc, char *Fmt, va_list VA);
void errorAt(char *Loc, char *Fmt, ...);
void errorTok(Token *Tok, char *Fmt, ...);

bool equal(Token *Tok, char *Str);
Token *skip(Token *Tok, char *Str);

// AST的节点类型
typedef enum
{
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
    ND_NEG,
    ND_EQ, // ==
    ND_NE, // !=
    ND_LT, // <
    ND_LE, // <=
    ND_GT, // >
    ND_GE, // >=
    ND_ASSIGN, // 赋值
    ND_EXPR_STMT, // 表达式语句
    ND_VAR, // 变量
} NodeKind;


typedef struct Node Node;
struct Node
{
    NodeKind Kind; // node kind
    Node *LHS;     // left-hand side
    Node *RHS;     // right-hand side
    Node *Next;    // 指向下一个语句  
    char Name;     // 存储ND_VAR的字符串
    int Val;
};

Node *parse(Token* Tok);
void codegen(Node* Nd);