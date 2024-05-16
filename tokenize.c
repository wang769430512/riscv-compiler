#include "rvcc.h"

// 输入的文件名
static char *CurrentFilename;

// 输入的字符串
static char *CurrentInput;

void error(char *Fmt, ...) {
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

static void verrorAt(char *Loc, char *Fmt, va_list VA) {
/*
    char *Line = Loc;
    while (CurrentInput < Line && Line[-1] != '\n') {
        Line--;
    }

    char *End = Loc;
    while (*End != '\n') {
        End++;
    }

    int LineNo = 1;
    for (char *P = CurrentInput; P < Line; P++) {
        if (*P == '\n') {
            LineNo++;
        }
    }

    int Indent = fprintf(stderr, "%s:%d", CurrentFilename, LineNo);
    fprintf(stderr, "%.*s", (int)(End - Line), Line);
    int Pos = Line - Loc + Indent;
*/
    // 查找包含loc的行
    char *Line = Loc;
    // Line递减到当前行的最开始的位置
    // Line<CurrentInput, 判断是否读取到文件最开始的位置
    // Line[-1] != '\n', Line字符串前一个字符是否为换行符(上一行末尾)
    while (CurrentInput < Line && Line[-1] != '\n') {
        Line--;
    }

    // End递增到行位的换行符
    char *End = Loc;
    while (*End != '\n') {
        End++;
    }

    // 获取行号
    int LineNo = 1;
    for (char *P = CurrentInput; P < Line; P++) {
        // 遇到换行符则行号+1
        if (*P == '\n') {
            LineNo++;
        }
    }

    // 输出 文件名:错误行
    // Indent记录输出了多少个字符
    int Indent = fprintf(stderr, "%s:%d: ", CurrentFilename, LineNo);
    // 输出Line的行内所有字符(不含换行符)
    fprintf(stderr, "%.*s\n", (int)(End - Line), Line);
    // 计算错误信息位置，在当前行内的偏移量+前面输出了多少个字符
    int Pos = Loc - Line + Indent;

    //int Pos = Loc - CurrentInput;

    fprintf(stderr, "%*s", Pos, "");
    fprintf(stderr, "^ ");
    vfprintf(stderr, Fmt, VA);
    fprintf(stderr, "\n");
    va_end(VA);
}

void errorAt(char *Loc, char *Fmt, ...) {
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Loc, Fmt, VA);
    exit(1);
}

void errorTok(Token *Tok, char *Fmt, ...) {
    va_list VA;
    va_start(VA, Fmt);
    verrorAt(Tok->Loc, Fmt, VA);
    exit(1);
}

bool equal(Token *Tok, char *Str) {

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

// 消耗掉指定Token
bool consume(Token **Rest, Token *Tok, char *Str) 
{
    // exist
    if (equal(Tok, Str)) {
        *Rest = Tok->Next;
        return true;
    }

    // no exist
    *Rest = Tok;
    return false;
}

static int getNumber(Token *Tok)
{
    if (Tok->Kind != TK_NUM)
    {
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

static bool startsWith(char *Str, char *SubStr) {
    return strncmp(Str, SubStr, strlen(SubStr)) == 0;
}

static bool isIdent1(char C) {
    return ('a' <= C && C <= 'z') || ('A' <= C && C <= 'Z') || C == '_'; 
}

static bool isIdent2(char C) {
    return isIdent1(C) || ('0' <= C && C <= '9');
}

static int readPunct(char *Ptr) {
    if (startsWith(Ptr, "==") || startsWith(Ptr, "!=") || startsWith(Ptr, "<=") \
     || startsWith(Ptr, ">=")) {
        return 2;
    }

    return (ispunct(*Ptr)) ? 1:0;    
} 

static bool isKeyword(Token *Tok) {
    static char* KW[] = {"return", "if", "else", "for", "while", "int", "sizeof", "char"};
    
    for (int I=0; I < sizeof(KW)/sizeof(*KW); I++) {
        if (equal(Tok, KW[I])) {
            return true;
        }
    }
    // for (char * kw = KW[0]; kw; kw++)
    // if (equal(Tok, kw)) {
    //     return true;
    // } 

    return false;
}

// 读取到字符串字面量结尾
static char *stringLiteralEnd(char *P) {
    char *Start = P;
    for (; *P != '"'; P++) {  
        if (*P == '\n' || *P == '\0') {
            errorAt(Start, "unclosed string literal");
        }

        if (*P == '\\') {
            P++;
        }
    }
    return P;
}

static int fromHex(char C) {
    if ('0' <= C && C <= '9') {
        return C - '0';
    } else if ('a' <= C && C <= 'f') {
        return C - 'a' + 10;
    } 
    return C - 'A' + 10;
}

static int readEscapedChar(char **NewPos, char *P) {
    if (*P >= '0' && *P <= '7') {
        // 读取一个八进制数字，不能长于三位
        //\abc = (a*8+b)*8+c
        int C = *P++ - '0';
        if (*P >= '0' && *P <= '7') {
            C = (C << 3) + (*P++ - '0');

            if (*P >= '0' && *P <= '7') {
                C = (C << 3) + (*P++ - '0');
            }
        }
        *NewPos = P;
        return C;
    }

    if (*P == 'x') {
        P++;
        // 判断是否为十六进制数字
        if (!isxdigit(*P)) {
            errorAt(P, "invalid hex escape sequence");
        }
        int C = 0;
        // 读取一位或多位十六进制数字
        // \xWXYZ = ((W*16+X)*16+Y)*16+Z
        for (; isxdigit(*P); P++) {
            C = (C << 4) + fromHex(*P);
        }

        *NewPos = P;
        return C;
    }

        
        
    *NewPos = P + 1;

    switch(*P) {
    case 'a': // 响铃 (警报)
        return '\a';
    case 'b': // 退格
        return '\b';
    case 't': // 水平指标符
        return '\t';
    case 'n': // 换行
        return '\n';
    case 'v': // 垂直制表符
        return '\v';
    case 'f': // 换页
        return '\f';
    case 'r': // 回车
        return '\r';
    // 属于GNU C拓展
    case 'e': // 转义符
        return 27;
    default:  // 默认将原字符返回
        return *P;
    }
}

static Token *readStringLiteral(char *Start) {
    // 读取到字符串字面量内的最后一个双引号的位置
    char *End = stringLiteralEnd(Start + 1);
    // 定义一个与字符串字面量内字符数相同的Buf
    char *Buf = calloc(1, End - Start);
    int Len = 0;

    // 将读取后的结果写入Buf
    for (char *P = Start + 1; P < End;) {
        if (*P == '\\') {
            Buf[Len++] = readEscapedChar(&P, P + 1);
            //P += 2;
        } else {
            Buf[Len++] = *P++;
        }
    }

    // Token这里需要包含带双引号的字符串字面量
    Token *Tok = newToken(TK_STR, Start, End + 1);
    // 为\0增加一位
    Tok->Ty = arrayOf(TyChar, Len +1);
    Tok->Str = Buf;
    return Tok;
}

static void convertKeywords(Token *Tok) {
    for (Token *T = Tok; T->Kind != TK_EOF; T = T->Next) {
        if (isKeyword(T)) {
            T->Kind = TK_KEYWORD;
        }
    }
}

// 终结符解析，文件名，文件内容
Token *tokenize(char* Filename, char *P) {
    CurrentFilename = Filename;
    CurrentInput = P;
    Token Head = {};
    Token *Cur = &Head;

    while (*P) {
        // 跳过行注释
        if (startsWith(P, "//")) {
            P += 2;
            while (*P != '\n') {
                P++;
            }
            continue;
        }

        // 跳过块注释
        if (startsWith(P, "/*")) {
            char *Q = strstr(P, "*/");
            if (!Q) {
                errorAt(P, "unclosed the block comment");
            }
            P = Q + 2;
            continue;
        }
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

        // 解析字符串字面量
        if (*P == '"') {
            Cur->Next = readStringLiteral(P);
            Cur = Cur->Next;
            P += Cur->Len;
            continue;
        }

        if (isIdent1(*P)) {
            char *Start = P;
            do {
              ++P;  
            } while (isIdent2(*P));
            Cur->Next = newToken(TK_IDENT, Start, P);
            Cur = Cur->Next;
            
            continue;
        }

        int PunctLen = readPunct(P);
        if (PunctLen) {
            Cur->Next = newToken(TK_PUNCT, P, P + PunctLen);
            Cur = Cur->Next;
            P += PunctLen;
            continue;
        }
                  
        errorAt(P, "invalid token");
    }
    Cur->Next = newToken(TK_EOF, P, P);
    convertKeywords(Head.Next);
    return Head.Next;
}

/*
static char *readFile(char *Path) {
    FILE *FP;
    if (strcmp(Path, "-") == 0) {
        FP = stdin;
    } else {
        FP = fopen(Path, "r");
    }

    char *Buf;
    size_t BufLen;
    FILE *Out = open_memstream(&Buf, &BufLen);

    while (true) {
        char Buf2[4096];
        int N = fread(Buf2, 1, sizeof(Buf2), FP);
        if (N == 0) {
            break;
        }

        fwrite(Buf, 1, N, Out);
    }
    if (FP != stdin) {
        fclose(FP);
    }
    
    fflush(Out);
    if (BufLen == 0 || Buf[BufLen - 1] == '\n') {
        fputc('\n', Out);
    }

    fputc('\0', Out);
    fclose(Out);
    return Buf;
}
*/

static char *readFile(char *Path) {
    FILE *FP;

    if (strcmp(Path, "-") == 0) {
        // 如果文件名是"-",那么就从输入中读取
        FP = stdin;
    } else {
        FP = fopen(Path, "r");
        if (!FP) {
            // errno为系统最后一次的错误代码
            // strerror以字符串的形式输出错误代码
            error("Cannot open %s : %s", Path, strerror(errno));
        }
    }

    // 要返回的字符串
    char *Buf;
    size_t BufLen;
    FILE *Out = open_memstream(&Buf, &BufLen);

    // 读取整个文件
    while (true) {
        char Buf2[4096];
        // fread从文件流中读取数据到数组中
        // 数组指针Buf2，数组元素大小1，数组元素个数4096，文件流指针
        int N = fread(Buf2, 1, sizeof(Buf2), FP);
        if (N == 0) {
            break;
        }

        // 数组指针Buf2,数组元素大小1，实际元素个数N，文件流指针
        fwrite(Buf2, 1, N, Out);
    }

    // 对文件完成了读取
    if (FP != stdin) {
        fclose(FP);
    }

    // 刷新流的输出缓冲区，确保内容都被输出到流中
    fflush(Out);
    // 确保最后一行以'\n'结尾
    if (BufLen == 0 || Buf[BufLen - 1] != '\n') {
        // 将字符输出到流中
        fputc('\n', Out);
    }
    fputc('\0', Out);
    fclose(Out);
    return Buf;
}

// 对文件进行词法分析
Token *tokenizeFile(char *Path) { return tokenize(Path, readFile(Path)); }