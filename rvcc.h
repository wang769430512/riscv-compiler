// 使用POSIX.1标准
// 使用了strndup函数

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define _POSIX_C_SOURCE 200809L

typedef struct Type Type;
typedef struct Node Node;
typedef struct Member Member;

//
// 字符串
//

char *format(char *Fmt, ...);

typedef enum
{
    TK_IDENT,   // 标记符，可以为变量名、函数名
    TK_PUNCT,   // 操作符如: + -
    TK_KEYWORD, // 关键字
    TK_STR,     // 字符串字面量
    TK_NUM,     // 数字 
    TK_EOF,     // 文件终止符，即文件的最后
} TokenKind;

typedef struct Token Token;
struct Token
{
    TokenKind Kind; // 种类
    Token *Next;    // 指向下一个终结符
    int Val;        // 值
    char *Loc;      // 在解析的字符串内的位置
    int Len;        // 长度
    Type *Ty;       // TK_STR使用
    char *Str;      // 字符串字面量，包括'\0'

    int LineNo;     // 行号
};

void error(char *Fmt, ...);
//void verrorAt(char *Loc, char *Fmt, va_list VA);
void errorAt(char *Loc, char *Fmt, ...);
void errorTok(Token *Tok, char *Fmt, ...);

bool equal(Token *Tok, char *Str);
Token *skip(Token *Tok, char *Str);
bool consume(Token **Rest, Token *Tok, char *Str);
// 词法分析
Token *tokenizeFile(char *Path);

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
    ND_COMMA,  // , 逗号
    ND_MEMBER, // . 结构体成员访问
    ND_IF,
    ND_FOR, // "for" or "while"
    ND_BLOCK,  // { ... }, 代码块
    ND_FUNCALL,  //函数调用
    ND_EXPR_STMT, // 表达式语句
    ND_STMT_EXPR, // 语句表达式
    ND_VAR, // 变量
} NodeKind;

typedef struct Obj Obj;
struct Obj {
    Obj *Next;    // 指向下一对象
    char *Name;   // 变量名
    Type *Ty;     // 变量类型
    bool IsLocal; // 是局部或全局变量
    
    // 局部变量
    int Offset;  // fp的偏移量

    // 函数 或 全局变量
    bool isFunction;

    // 全局变量
    char *InitData;

    // 函数
    Obj *Params;   // 形参

    // 代码块 或 语句表达式
    Node *Body;    
    Obj *Locals;   // 本地变量
    int StackSize; // 栈大小
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

    Member *Mem;

    // 函数调用
    char *FuncName; // 函数名
    Node *Args;     // 函数参数
    
    Obj *Var;     // 存储ND_VAR的字符串
    int Val;
};

Obj *parse(Token *Tok);

// 类型种类
typedef enum {
    TY_CHAR,   // char类型
    TY_INT,    // int整形
    TY_PTR,    // 指针
    TY_FUNC,   // 函数
    TY_ARRAY,  // 数组
    TY_STRUCT, // 结构体
    TY_UNION,  // 联合体
} TypeKind;

struct Type {
    TypeKind Kind; // 种类
    int Size;      // 大小，sizeof返回的值
    int Align;     // 对齐

    // 指针
    Type *Base; // 指向的类型

    // 类型对应名称，如:变量名、函数名
    Token *Name;    

    // 数组
    int ArrayLen; // 数组长度，元素总个数

    // 结构体
    Member *Mems;

    // 函数类型
    Type *ReturnTy; // 函数返回类型
    Type *Params;   // 形参
    Type *Next;     // 下一类型
};

// 结构体成员
struct Member {
    Member *Next; // 下一成员
    Type *Ty;     // 类型
    Token *Name;  // 名称
    int Offset;   // 偏移量
};

// 声明全局变量，在type.c中定义全局变量
extern Type *TyChar;
extern Type *TyInt;

bool isInteger(Type *Ty);
// 复制类型
Type *copyType(Type *Ty);
// 为节点内的所有节点添加类型
void addType(Node *Nd);
// 数组类型
Type *arrayOf(Type *Base, int Size);
// 函数类型
Type *funcType(Type *ReturnTy);

// 构建一个指针类型，并指向基类
Type *pointerTo(Type *Base);


// 代码生成入口函数
void codegen(Obj *Prog, FILE *Out);
int alignTo(int N, int Align);
