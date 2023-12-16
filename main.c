#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum
{
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

static char *CurrentInput;

static void error(char *Fmt, ...)
{
    // 定义一个va_list variable
    va_list VA;
    // VA获取Fmt后面的所有参数
    va_start(VA, Fmt);
    // vfprintf可以输出va_list类型的参数
    vfprintf(stderr, Fmt, VA);
    fprintf(stderr, "\n");
    // 清理VA
    va_end(VA);
    exit(1);
}

static void verrorAt(char *Loc, char *Fmt, va_list VA)
{
    fprintf(stderr, "%s\n", CurrentInput);

    int Pos = Loc - CurrentInput;

    fprintf(stderr, "%*s", Pos, " ");
    fprintf(stderr, "^ ");
    fprintf(stderr, Fmt, VA);
    fprintf(stderr, "\n");
    va_end(VA);
    exit(1);
}

static void errorAt(char *Loc, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Loc, Fmt, VA);
}

static void errorTok(Token *Tok, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Tok->Loc, Fmt, VA);
}

static bool equal(Token *Tok, char *Str)
{

    return memcmp(Tok->Loc, Str, Tok->Len) == 0 && Str[Tok->Len] == '\0';
}

static Token *skip(Token *Tok, char *Str)
{
    if (!equal(Tok, Str))
    {
        errorTok(Tok, "expect '%s'", Str);
    }
    return Tok->Next;
}

static int getNumber(Token *Tok)
{
    if (Tok->Kind != TK_NUM)
    {
        errorTok(Tok, "expect a number");
    }

    return Tok->Val;
}

static Token *newToken(TokenKind Kind, char *Start, char *End)
{
    // 分配1个Token的内存空间
    Token *Tok = calloc(1, sizeof(Token));
    Tok->Kind = Kind;
    Tok->Loc = Start;
    Tok->Len = End - Start;
    return Tok;
}

static bool startWith(char *Str, char *SubStr) {
    return strncmp(Str, SubStr, strlen(SubStr)) == 0;
}

static int readPunct(char *Ptr) {
    if (startWith(Ptr, "==") || startWith(Ptr, "!=") || startWith(Ptr, ">=") \
       || startWith(Ptr, "<=")) {
        return 2;
    }

    return (ispunct(*Ptr)) ? 1:0;    
}

static Token *tokenize()
{
    char *P = CurrentInput;
    Token Head = {};
    Token *Cur = &Head;

    while (*P)
    {
        // 跳过所有空白符，如：空白、回车
        if (isspace(*P))
        {
            ++P;
            continue;
        }

        if (isdigit(*P))
        {
            Cur->Next = newToken(TK_NUM, P, P);
            Cur = Cur->Next;
            const char *OldPtr = P;
            Cur->Val = strtoul(P, &P, 10);
            Cur->Len = P - OldPtr;
            continue;
        }

        int PunctLen = readPunct(P);
        if (PunctLen > 0) {
            Cur->Next = newToken(TK_PUNCT, P, P+PunctLen);
            Cur = Cur->Next;
            P += PunctLen;
            continue;
        }
                  
        errorAt(P, "Invalid token");
    }
    Cur->Next = newToken(TK_EOF, P, P);

    return Head.Next;
}

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
} NodeKind;

typedef struct Node Node;
struct Node
{
    NodeKind Kind; // node kind
    Node *LHS;     // left-hand side
    Node *RHS;     // right-hand side
    int Val;
};

static Node *newNode(NodeKind Kind)
{
    Node *Nd = calloc(1, sizeof(Node));
    Nd->Kind = Kind;
    return Nd;
}

static Node *newNum(int Val)
{
    Node *Nd = newNode(ND_NUM);
    Nd->Val = Val;
    return Nd;
}

// create a new binary tree
static Node *newBinary(NodeKind Kind, Node *LHS, Node *RHS)
{
    Node *Nd = newNode(Kind);
    Nd->LHS = LHS;
    Nd->RHS = RHS;

    return Nd;
}

// create a new unary tree
static Node *newUnary(NodeKind Kind, Node *child)
{
    Node *Nd = newNode(Kind);
    Nd->LHS = child;

    return Nd;
}

// expr = equality
// equality = relational("==" relational | "!=" relational )*
// relational = add("<=" add | "<" add | ">=" add | ">" add)*
// add = mul("+"mul | "-"mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-") unary | primary
// primary = "("expr")" | num
static Node *expr(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

// expr = equality
static Node *expr(Token **Rest, Token *Tok) {
    return equality(Rest, Tok);
}

// equality = relational("==" relational | "!=" relational)
static Node *equality(Token **Rest, Token *Tok) {
    Node *Nd = relational(&Tok, Tok);

    while (true) {
        if (equal(Tok, "==")) {
            Nd = newBinary(ND_EQ, Nd, relational(&Tok, Tok->Next));
            continue;            
        }

        if (equal(Tok, "!=")) {
            Nd = newBinary(ND_NE, Nd, relational(&Tok, Tok->Next));
            continue;
        }

        *Rest = Tok;
        return Nd;
    } 
}

// relational = add("<=" add | "<" add | ">=" add | ">" add)*
static Node *relational(Token **Rest, Token *Tok) {
    Node *Nd = add(&Tok, Tok);
    while (true) {
        if (equal(Tok, "<=")) {
            Nd = newBinary(ND_LE, Nd, add(&Tok, Tok->Next));
            continue;
        }

        if (equal(Tok, "<")) {
            Nd = newBinary(ND_LT, Nd, add(&Tok, Tok->Next));
            continue;
        }

        // x >= y
        // x < y
        if (equal(Tok, ">=")) {
            Nd = newBinary(ND_LT, add(&Tok, Tok->Next), Nd);
            continue;
        }

        if (equal(Tok, ">")) {
            Nd = newBinary(ND_LE, add(&Tok, Tok->Next), Nd);
            continue;
        }

        *Rest = Tok;
        return Nd;
    }
}

// add = mul("+"mul | "-"mul)*
static Node *add(Token **Rest, Token *Tok)
{
    Node *Nd = mul(&Tok, Tok);

    while (true)
    {
        if (equal(Tok, "+"))
        {
            Nd = newBinary(ND_ADD, Nd, mul(&Tok, Tok->Next));
            continue;
        }

        if (equal(Tok, "-"))
        {
            Nd = newBinary(ND_SUB, Nd, mul(&Tok, Tok->Next));
            continue;
        }

        *Rest = Tok;
        return Nd;
    }

    errorTok(Tok, "expected an expression");

    return NULL;
}

// mul = unary("*" unary | "/" unary)
static Node *mul(Token **Rest, Token *Tok)
{
    Node *Nd = unary(&Tok, Tok);

    while (true)
    {
        if (equal(Tok, "*"))
        {
            Nd = newBinary(ND_MUL, Nd, unary(&Tok, Tok->Next));
            continue;
        }

        if (equal(Tok, "/"))
        {
            Nd = newBinary(ND_DIV, Nd, unary(&Tok, Tok->Next));
            continue;
        }

        *Rest = Tok;
        return Nd;
    }
}

// unary = unary("+" | "-")unary | primary
static Node *unary(Token **Rest, Token *Tok)
{
    // "+"" unary
    if (equal(Tok, "+"))
    {
        return unary(Rest, Tok->Next);
    }

    // "-" unary
    if (equal(Tok, "-"))
    {
        return newUnary(ND_NEG, unary(Rest, Tok->Next));
    }

    // primary
    return primary(Rest, Tok);
}

// primary = "("expr")" | num
static Node *primary(Token **Rest, Token *Tok)
{
    if (equal(Tok, "("))
    {
        Node *Nd = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ")");
        return Nd;
    }

    if (Tok->Kind == TK_NUM)
    {
        Node *Nd = newNum(Tok->Val);
        *Rest = Tok->Next;
        return Nd;
    }

    errorTok(Tok, "expected an expression");
    return NULL;
}

static int Depth;

static void push(void)
{
    printf("  addi sp, sp, -8\n");
    printf("  sd a0, 0(sp)\n");
    Depth++;
}

static void pop(char *Reg)
{
    printf("  ld %s, 0(sp)\n", Reg);
    printf("  addi sp, sp, 8\n");
    Depth--;
}

static void genExpr(Node *Nd)
{
    switch (Nd->Kind) {
    case ND_NUM:
        printf("  li a0, %d\n", Nd->Val);
        return;
    case ND_NEG: // -- a0 = -a1;
        genExpr(Nd->LHS);
        printf("  neg a0, a0\n");
        return;
    default:
        break;
    }
    // 递归到最右节点
    genExpr(Nd->RHS);
    // 将结果入栈
    push();
    // 递归到左节点
    genExpr(Nd->LHS);
    // 将结果弹到a1
    pop("a1");

    // 生成各个二叉树节点
    switch (Nd->Kind)
    {
    case ND_NUM:
        printf("  li a0, %d\n", Nd->Val);
        return;
    case ND_NEG: // -- a0 = -a1;
        genExpr(Nd->LHS);
        printf("  neg a0, a0\n");
        return;
    case ND_ADD: // + a0 = a0 + a1;
        printf("  add a0, a0, a1\n");
        return;
    case ND_SUB: // - a0 = a0 - a1;
        printf("  sub a0, a0, a1\n");
        return;
    case ND_MUL: // * a0 = a0 * a1;
        printf("  mul a0, a0, a1\n");
        return;
    case ND_DIV: // / a0 = a0 / a1;
        printf("  div a0, a0, a1\n");
        return;
    case ND_EQ:
    case ND_NE:
        // a0 = a0 ^ a1
        printf("  xor a0, a0, a1\n");
        if (Nd->Kind == ND_EQ) {
            printf("  seqz a0, a0\n");
        } else {
            printf("  snez a0, a0\n");
        }
        return;
    case ND_LT:
        printf("  slt a0, a0, a1\n");
        return;
    case ND_LE:
        // a0 <= a1
        // a0=a1<a0, a0=a0^1
        printf("  slt a0, a1, a0\n");
        printf("  xori a0, a0, 1\n");
        return;
    default:
        break;
    }

    error("invalid expression");
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        error("%s: invalid number of arguments", argv[0]);
    }

    CurrentInput = argv[1];
    Token *Tok = tokenize();

    Node *Node = expr(&Tok, Tok);

    if (Tok->Kind != TK_EOF)
    {
        errorTok(Tok, "extra token");
    }

    printf("  .globl main\n");
    printf("main:\n");

    // 遍历AST树生成汇编
    genExpr(Node);

    printf("  ret\n");

    // 如果栈未清空，则报错
    assert(Depth == 0);

    return 0;
}
