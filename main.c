#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef enum{
    TK_PUNCT,
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Token Token;

struct Token{
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
    fprintf(stderr,"\n");
    // 清理VA
    va_end(VA);
    exit(1);
}

static void verrorAt(char *Loc, char *Fmt, va_list VA) {
    fprintf(stderr, "%s\n", CurrentInput);

    int Pos = Loc - CurrentInput;

    fprintf(stderr, "%*s", Pos, " ");
    fprintf(stderr, "^ ");
    fprintf(stderr, Fmt, VA);
    fprintf(stderr, "\n");
    va_end(VA);
    exit(1);
}

static void errorAt(char *Loc, char *Fmt, ...) {
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Loc, Fmt, VA);
}

static void errorTok(Token *Tok, char *Fmt, ...) {
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Tok->Loc, Fmt, VA);
}

static bool equal(Token *Tok, char *Str) {

    return memcmp(Tok->Loc, Str, Tok->Len)==0 && Str[Tok->Len]=='\0';
}

static Token *skip(Token *Tok, char *Str) {
    if (!equal(Tok, Str)) {
        errorTok(Tok, "expect '%s'", Str);
    }
    return Tok->Next;
}

static int getNumber(Token* Tok) {
    if (Tok->Kind != TK_NUM) {
        errorTok(Tok, "expect a number");
    }

    return Tok->Val;
}

static Token *newToken(TokenKind Kind, char *Start, char *End) {
    // 分配1个Token的内存空间
    Token *Tok = calloc(1, sizeof(Token));
    Tok->Kind = Kind;
    Tok->Loc = Start;
    Tok->Len = End - Start;
    return Tok;
}

static Token *tokenize() {
    char *P = CurrentInput;
    Token Head = {};
    Token *Cur = &Head;

    while (*P) {
        // 跳过所有空白符，如：空白、回车
        if (isspace(*P)) {
            ++P;
            continue;
        }

        if (isdigit(*P)) {
            Cur->Next = newToken(TK_NUM, P, P);
            Cur = Cur->Next;
            const char *OldPtr = P;
            Cur->Val = strtoul(P, &P, 10);
            Cur->Len = P - OldPtr;
            continue;
        }

        if (ispunct(*P)) {
            // 操作符长度都为1
            Cur->Next = newToken(TK_PUNCT, P, P + 1);
            Cur = Cur->Next;
            ++P;
            continue;
        }
        errorAt(P, "Invalid token");
    }
    Cur->Next = newToken(TK_EOF, P, P);

    return Head.Next;
}

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind Kind; // node kind
    Node *LHS; // left-hand side
    Node *RHS; // right-hand side
    int Val; 
};

static Node *newNode(NodeKind Kind) {
    Node *Nd = calloc(1, sizeof(Node));
    Nd->Kind = Kind;
    return Nd;
}

static Node *newNum(int Val) {
    Node* Nd = newNode(ND_NUM);
    Nd->Val = Val;
    return Nd;
}



// create a new binary tree
static Node *newBinary(NodeKind Kind, Node* LHS, Node* RHS) {
    Node *Nd = newNode(Kind);
    Nd->LHS = LHS;
    Nd->RHS = RHS;

    return Nd;
}

// expr = mul("+"mul | "-"mul)*
// mul = primary ("*" primary | "/" primary)*
// primary = "("expr")" | num

static Node *expr(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

// expr = mul("+"mul | "-"mul)*
static Node *expr(Token **Rest, Token *Tok) {
    Node *Nd = mul(&Tok, Tok);

    while (true) {
        if (equal(Tok, "+")) {
            Nd = newBinary(ND_ADD, Nd, mul(&Tok, Tok->Next));
            continue;
        }

        if (equal(Tok, "-")) {
            Nd = newBinary(ND_SUB, Nd, mul(&Tok, Tok->Next));
            continue;
        }

        *Rest = Tok;
        return Nd;
    }

    errorTok(Tok, "expected an expression");

    return NULL;
}

// mul = primary("*" primary | "/" primary)
static Node *mul(Token **Rst, Token *Tok) {
    Node *Nd = primary(&Tok, Tok);

    while (true) {
        if (equal(Tok, "*")) {
            Nd = newBinary(ND_MUL, Nd, primary(&Tok, Tok->Next));
            continue;
        }

        if (equal(Tok, "/")) {
            Nd = newBinary(ND_DIV, Nd, primary(&Tok, Tok->Next));
            continue;
        }

        *Rst = Tok;
        return Nd;
    }
}

// primary = "("expr")" | num
static Node *primary(Token **Rest, Token *Tok) {
    if (equal(Tok, "(")) {
        Node *Nd = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ")");
        return Nd;
    }

    if (Tok->Kind == TK_NUM) {
        Node *Nd = newNum(Tok->Val);
        *Rest = Tok->Next;
        return Nd;
    }

    errorTok(Tok, "expected an expression");
    return NULL;
}

static int Depth;

static void push(void) {
    printf("  addi sp, sp, -8\n");
    printf("  sd a0, 0(sp)\n");
    Depth++;
}

static void pop(char *Reg) {
    printf("  ld %s, 0(sp)\n", Reg);
    printf("  addi sp, sp, 8\n");
    Depth--;
}

static void genExpr(Node *Nd) {
    if (Nd->Kind == ND_NUM) {
        printf("  li a0, %d\n", Nd->Val);
        return;
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
    switch (Nd->Kind) {
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
        default:
            break;
    }

    error("invalid expression");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        error("%s: invalid number of arguments", argv[0]);
    }

    CurrentInput = argv[1];
    Token *Tok = tokenize();

    Node *Node = expr(&Tok, Tok);

    if (Tok->Kind != TK_EOF) {
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
