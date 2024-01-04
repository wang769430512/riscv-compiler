#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#define _POSIX_C_SOURCE 200809L

typedef enum
{
    TK_IDENT, // 标记符，可以为变量名、函数名
    TK_PUNCT,
    TK_KEYWORD,
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Node Node;

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
    ND_ADDR,   // 取地址 &
    ND_DEREF,  // 解引用 *
    ND_RETURN, // return
    ND_ASSIGN, // 赋值
    ND_IF,
    ND_FOR, // "for" or "while"
    ND_BLOCK,  // { ... }, 代码块
    ND_EXPR_STMT, // 表达式语句
    ND_VAR, // 变量
} NodeKind;

typedef struct Obj Obj;
struct Obj {
    Obj *Next;
    char *Name;
    int offset;
};

struct Node
{
    NodeKind Kind; // node kind
    Node *Next;    // 指向下一个语句  
    Token *Tok;
    
    Node *LHS;     // left-hand side
    Node *RHS;     // right-hand side

    // "if"语句 or "for"语句
    Node *Cond;
    Node *Then;
    Node *Els;
    Node* Init;
    Node* Inc;

    // 代码块
    Node *Body;
    
    Obj *Var;     // 存储ND_VAR的字符串
    int Val;
};

// function
typedef struct Function Function;
struct Function {
    Node *Body;
    Obj *Locals;
    int StackSize;
};

Function *parse(Token *Tok);
void codegen(Function *Prog);
