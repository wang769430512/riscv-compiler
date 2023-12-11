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

static int isspace(char ch) {
    if (ch == ' ') {
        return 1;
    }
    return 0;
}

static int isdigit(char ch) {
    if (ch >= '0' && ch <='9') {
        return 1;
    }
    return 0;
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

        if (*P == '+' || *P == '-') {
            // 操作符长度都为1
            Cur->Next = newToken(TK_PUNCT, P, P + 1);
            Cur = Cur->Next;
            const char *oldPtr = P;
            ++P;
            continue;
        }
        errorAt(P, "Invalid token");
    }
    Cur->Next = newToken(TK_EOF, P, P);

    return Head.Next;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        error("%s: invalid number of arguments", argv[0]);
    }

    CurrentInput = argv[1];
    Token *Tok = tokenize();

    printf("  .globl main\n");
    printf("main:\n");
    printf("  li a0, %d\n", Tok->Val);
    Tok=Tok->Next;
    while (Tok->Kind != TK_EOF) {
        if (equal(Tok, "+")) {
            Tok = Tok->Next;
            printf("  addi a0, a0, %d\n", getNumber(Tok));
            Tok = Tok->Next;  
            continue;
        } 

        Tok = skip(Tok, "-");
        printf("  addi a0, a0, -%d\n", getNumber(Tok));
        Tok = Tok->Next;
    }

    printf("  ret\n");

    return 0;
}