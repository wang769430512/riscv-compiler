#include "rvcc.h"

static char *CurrentInput;

void error(char *Fmt, ...)
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
}

void errorAt(char *Loc, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Loc, Fmt, VA);
    exit(1);
}

void errorTok(Token *Tok, char *Fmt, ...)
{
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Tok->Loc, Fmt, VA);
    exit(1);
}

bool equal(Token *Tok, char *Str)
{

    return memcmp(Tok->Loc, Str, Tok->Len) == 0 && Str[Tok->Len] == '\0';
}

Token *skip(Token *Tok, char *Str)
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

static Token *newToken(TokenKind Kind, char *Start, char *End)
{
    // 分配1个Token的内存空间
    Token *Tok = calloc(1, sizeof(Token));
    Tok->Kind = Kind;
    Tok->Loc = Start;
    Tok->Len = End - Start;
    return Tok;
}

Token *tokenize(char *P)
{
    CurrentInput = P;
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

        if ('a' <= *P && *P <= 'z') {
            Cur->Next = newToken(TK_IDENT, P, P+1);
            Cur = Cur->Next;
            ++P;
            continue;
        }

        int PunctLen = readPunct(P);
        if (PunctLen) {
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
