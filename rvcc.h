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

typedef struct Type Type;
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
bool consume(Token **Rest, Token *Tok, char *Str);
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
    ND_FUNCALL,  //函数调用
    ND_EXPR_STMT, // 表达式语句
    ND_VAR, // 变量
} NodeKind;

typedef struct Obj Obj;
struct Obj {
    Obj *Next;
    char *Name;
    Type *Ty;   // 变量类型
    int Offset;
};

struct Node
{
    NodeKind Kind; // node kind
    Node *Next;    // 指向下一个语句  
    Token *Tok;
    Type *Ty;      // 节点中数据的类型
    
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

    // 函数调用
    char *FuncName; // 函数名
    Node *Args;     // 函数参数
    
    Obj *Var;     // 存储ND_VAR的字符串
    int Val;
};

// function
typedef struct Function Function;
struct Function {
    Function *Next; // 下一个函数
    char *Name;     // 函数名
    Obj *Params;    // 形参

    Node *Body;     // 函数体
    Obj *Locals;    // 本地变量
    int StackSize;  // 栈大小
};

Function *parse(Token *Tok);

// 类型种类
typedef enum {
    TY_INT,  // int整形
    TY_PTR,  // 指针
    TY_FUNC, // 函数
} TypeKind;

struct Type {
    TypeKind Kind; // 种类

    // 指针
    Type *Base; // 指向的类型

    // 声明
    Token *Name;    

    // 函数类型
    Type *ReturnTy; // 函数返回类型
    Type *Params;   // 形参
    Type *Next;     // 下一类型
};

extern Type *TyInt;

bool isInteger(Type *Ty);
// 复制类型
Type *copyType(Type *Ty);
// 为节点内的所有节点添加类型
void addType(Node *Nd);
// 函数类型
Type *funcType(Type *ReturnTy);

// 构建一个指针类型，并指向基类
Type *pointerTo(Type *Base);
void codegen(Function *Prog);
