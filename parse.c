#include "rvcc.h"

// 局部和全局变量的域
typedef struct VarScope VarScope;
struct VarScope {
    VarScope *Next; // 下一变量域
    char *Name;     // 变量域名称  
    Obj *Var;       // 对应的变量
};

// 结构体标签的域
typedef struct TagScope TagScope;
struct TagScope {
    TagScope *Next; // 下一标签域
    char *Name;      // 域名称
    Type *Ty;        // 域类型
};

// 表示一个块域
typedef struct Scope Scope;
struct Scope {
    Scope *Next;         // 指向上一级的域

    // C有两个域:变量域，结构体标签域
    VarScope *Vars;
    TagScope *Tags;

};

// 在解析时，全部的变量实例都被累加到这个列表里
Obj *Locals;  // 局部变量
Obj *Globals; // 全局变量

// 所有的域的链表
static Scope *Scp = &(Scope){};

// 通过Token查找标签
static Type *findTag(Token *Tok) {
    for (Scope *S = Scp; S; S=S->Next) {
        for (TagScope *S2 = S->Tags; S2; S2=S2->Next) {
            if (equal(Tok, S2->Name)) {
                return S2->Ty;
            }
        }
    }
    return NULL;
}

static Node *newNode(NodeKind Kind, Token *Tok)
{
    Node *Nd = calloc(1, sizeof(Node));
    Nd->Kind = Kind;
    Nd->Tok = Tok;
    return Nd;
}

static Node *newNum(int Val, Token *Tok)
{
    Node *Nd = newNode(ND_NUM, Tok);
    Nd->Val = Val;
    return Nd;
}

// create a new binary tree
static Node *newBinary(NodeKind Kind, Node *LHS, Node *RHS, Token *Tok)
{
    Node *Nd = newNode(Kind, Tok);
    Nd->LHS = LHS;
    Nd->RHS = RHS;

    return Nd;
}

static Node *newAdd(Node *LHS, Node *RHS, Token *Tok) {
    addType(LHS);
    addType(RHS);

    if (isInteger(LHS->Ty) && isInteger(RHS->Ty)) {
        return newBinary(ND_ADD, LHS, RHS, Tok);
    }

    if (LHS->Ty->Base && RHS->Ty->Base) {
        errorTok(Tok, "invalid operands");
    }

    // num + ptr -> ptr + num
    if (!LHS->Ty->Base && RHS->Ty->Base) {
        Node *Tmp = LHS;
        LHS = RHS;
        RHS = Tmp;
    }

    // ptr + num 
    // 指针加法，ptr+1这里的1不是一个字节，而是一个元素的空间, 所以需要*Size的操作
    RHS = newBinary(ND_MUL, RHS, newNum(LHS->Ty->Base->Size, Tok), Tok);

    return newBinary(ND_ADD, LHS, RHS, Tok);
}

static Node *newSub(Node *LHS, Node *RHS, Token *Tok) {
    addType(LHS);
    addType(RHS);

    // num - num
    if (isInteger(LHS->Ty) && isInteger(RHS->Ty)) {
        return newBinary(ND_SUB, LHS, RHS, Tok);
    }

    // ptr - num
    if (LHS->Ty->Base && isInteger(RHS->Ty)) {
        RHS = newBinary(ND_MUL, RHS, newNum(LHS->Ty->Base->Size, Tok), Tok);
        addType(RHS);
        Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
        Nd->Ty = LHS->Ty;
        return Nd;
    }
    
    // ptr - ptr 
    if (LHS->Ty->Base && RHS->Ty->Base) {
        Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
        Nd->Ty = TyInt;
        return newBinary(ND_DIV, Nd, newNum(LHS->Ty->Base->Size, Tok), Tok);
    }

    // num - ptr is wrong
    errorTok(Tok, "invalid operands");
    return NULL;
}

// create a new unary tree
static Node *newUnary(NodeKind Kind, Node *child, Token *Tok)
{
    Node *Nd = newNode(Kind, Tok);
    Nd->LHS = child;

    return Nd;
}

// 将变量存入当前的域中
static VarScope *pushScope(char *Name, Obj *Var) {
    VarScope *S = calloc(1, sizeof(VarScope));
    S->Name = Name;
    S->Var = Var;
    // 后来的在链表头部
    S->Next = Scp->Vars;
    Scp->Vars = S;
    return S;
}

// 新建变量
static Obj *newVar(char *Name, Type *Ty) {
    Obj *Var = calloc(1, sizeof(Obj));
    Var->Name = Name;
    Var->Ty = Ty;
    pushScope(Name, Var);
    return Var;
}

static Obj *newLVar(char *Name, Type *Ty) {
    Obj *Var = newVar(Name, Ty);
    Var->IsLocal = true;
    Var->Next = Locals;
    // 将变量插入头部
    Var->Next = Locals;
    Locals = Var;
    
    return Var;
}

// 在链表中新增一个全局变量
static Obj *newGVar(char *Name, Type *Ty) {
    Obj *Var = newVar(Name, Ty);
    Var->Next = Globals;
    Globals = Var;
    return Var;
}

static Node *newVarNode(Obj *Var, Token *Tok) {
    Node *Nd = newNode(ND_VAR, Tok);
    Nd->Var = Var;
    return Nd;
}

// program = functionDefinition*
// functionDefinition = declspec declarator "{" compoundStmt*
// declspec = "int" | "char" | structDecl
// declarator = "*"* ident typeSuffix
// typeSuffix = ("(" funcParams | "[" num "]" typeSuffix | ε
// funcParams = (param ("," "param")*)? ")"
// param = declspec declarator

// compondStmt = (declaration | stmt)* "}"
// declaration =
//    declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
// stmt =  ""return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)? 
//        | "for" "(" exprStmt expr?";" expr? ";" expr? ";")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compondStmt 
//        | exprStmt
// exprStmt = expr? ";"
// expr = assign ("," expr)?
// assign = equality ("=" assign)?
// equality = relational("==" relational | "!=" relational )*
// relational = add("<=" add | "<" add | ">=" add | ">" add)*
// add = mul("+"mul | "-"mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" | "*" | "/" | "*" | "&") unary | primary
// structMembers = (declspec declarator ("," declarator)* ";")*
// struct-union-decl = ident?("{" struct-members)?
// structDecl = ident? ("{" structMembers)?
// unionDecl = structUnionDecl
// postfix = primary ("[" expr "]" | "." ident)* |"->" ident)* 
// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")" 
//         | "sizeof" unary 
//         | ident func-args? 
//         | str 
//         | num

// funcall = ident "(" (assign ("," assign)*)? ")"
// // args = "(" ")"
//static Node *program(Token **Rest, Token *Tok);
static Type *declarator(Token **Rest, Token *Tok, Type *Ty);
static Node *compondStmt(Token **Rest, Token *Tok);
static Node *declaration(Token **Rest, Token *Tok);
static Node *stmt(Token **Rest, Token *Tok);
static Node *exprStmt(Token **Rest, Token *Tok);
static Node *expr(Token **Rest, Token *Tok);
static Node *assign(Token **Rest, Token *Tok);
static Node *equality(Token **Rest, Token *Tok);
static Node *relational(Token **Rest, Token *Tok);
static Node *add(Token **Rest, Token *Tok);
static Node *mul(Token **Rest, Token *Tok);
static Type *structDecl(Token **Rest, Token *Tok);
static Type *unionDecl(Token **Rest, Token *Tok);
static Node *unary(Token **Rest, Token *Tok);
static Node *postfix(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);
static Node *funCall(Token **Rest, Token *Tok);

// 进入域
static void enterScope(void) 
{
    Scope *S = calloc(1, sizeof(Scope));

    // 后面的在链表头部
    // 类似于栈的结构，栈顶对应最近的域
    S->Next = Scp;
    Scp = S;
}

// 结束当前域
static void leaveScope(void)
{
    Scp = Scp->Next;
}

// 通过名称，查找一个变量
static Obj *findVar(Token *Tok) {
    // 此处越先匹配的域，越深层
    for (Scope *S = Scp; S; S = S->Next)
        // 遍历域内的所有变量
        for (VarScope *S2 = S->Vars; S2; S2 = S2->Next)
            if (equal(Tok, S2->Name))
                return S2->Var;

    return NULL;
}
   
static bool isTypeName(Token *Tok) {
    return equal(Tok, "char") || equal(Tok, "int") || equal(Tok, "struct") || equal(Tok, "union");
}

// compondStmt = (declaration | stmt)* "}"
static Node *compondStmt(Token **Rest, Token *Tok) {
    Node *Nd = newNode(ND_BLOCK, Tok);
    
    // 这里使用了和词法分析类似的单向链表结构
    Node Head = {};
    Node *Cur = &Head;

    // 进入新的域
    enterScope();

    while (!equal(Tok, "}")) {     
        if (isTypeName(Tok)) {
            Cur->Next = declaration(&Tok, Tok);
        } else {
            Cur->Next = stmt(&Tok, Tok);
        }
        Cur = Cur->Next;
        // 构造完AST之后，添加节点类型
        addType(Cur);
    } 

    // 结束当前的域
    leaveScope();

    // Nd的Body存储了{}内解析的语句
    Nd->Body = Head.Next;
    *Rest = Tok->Next;

    return Nd;
}

// 新增唯一名称
static char* newUniqueName(void) {
    static int Id = 0;
    return format(".L..%d", Id++);
}

// 新增匿名全局变量
static Obj *newAnonGVar(Type *Ty) {
    return newGVar(newUniqueName(), Ty);
}

// 新增字符串字面量
static Obj *newStringLiteral(char *Str, Type *Ty) {
    Obj * Var = newAnonGVar(Ty);
    Var->InitData = Str;
    return Var;
}


// 获取标识符
static char *getIdent(Token *Tok) {
    if (Tok->Kind != TK_IDENT) 
        errorTok(Tok, "expected an identifier");

    return strndup(Tok->Loc, Tok->Len);
}

// 获取数字
static int getNumber(Token *Tok) {
    if (Tok->Kind != TK_NUM) {
        errorTok(Tok, "expected a number");
    }
    return Tok->Val;
}

// 将标签存入当前的域中
static void pushTagScope(Token *Tok, Type *Ty) {
    TagScope *S = calloc(1, sizeof(TagScope));
    S->Name = strndup(Tok->Loc, Tok->Len);
    S->Ty = Ty;
    S->Next = Scp->Tags;
    Scp->Tags = S;
}

// declspec = "int" | "char" | structDecl | unionDecl
// declarator specifier
static Type *declspec(Token **Rest, Token *Tok) {
    if (equal(Tok, "char")) {
        //*Rest = skip(Tok, "char");
        *Rest = Tok->Next;
        return TyChar;
    }

    // int
    if (equal(Tok, "int")) {
        *Rest = Tok->Next;
        return TyInt;
    }

    // structDecl
    if (equal(Tok, "struct")) {
        return structDecl(Rest, Tok->Next);
    }

    // unionDecl
    if (equal(Tok, "union")) {
        return unionDecl(Rest, Tok->Next);
    }

    errorTok(Tok, "expected typename");
    return NULL;
}

// funcParams = (param ("," param)*)? ")"
// param = declspec declarator
static Type *funcParams(Token **Rest, Token *Tok, Type *Ty) {
    Type Head = {};
    Type *Cur = &Head;

    while (!equal(Tok, ")")) {
        // funcParams = param ("," param)*
        // param = declspec declarator
        if (Cur != &Head) 
            Tok = skip(Tok, ",");

        Type *BaseTy = declspec(&Tok, Tok);
        Type *DeclarTy = declarator(&Tok, Tok, BaseTy);
        // 将类型复制到形参链表一份
        Cur->Next = copyType(DeclarTy);
        Cur = Cur->Next;
    }

    // 封装一个函数节点
    Ty = funcType(Ty);
    // 传递形参
    Ty->Params = Head.Next;
    *Rest = Tok->Next;
    return Ty;
}

// typeSuffix = ("funcParams | "["nums"]"" typeSuffix | ε
static Type *typeSuffix(Token **Rest, Token *Tok, Type *Ty) {
    // ("(" ")")?
    if (equal(Tok, "(")) {
        return funcParams(Rest, Tok->Next, Ty);
    }

    if (equal(Tok, "[")) {
        int Sz = getNumber(Tok->Next);
        Tok = skip(Tok->Next->Next, "]");
        Ty = typeSuffix(Rest, Tok, Ty);
        return arrayOf(Ty, Sz);
    }

    *Rest = Tok;
    return Ty;
}

// declarator = "*"* ident typeSuffix
static Type* declarator(Token **Rest, Token* Tok, Type *Ty) {
    // "*"*
    // 构建所有的(多重)指针
    while (consume(&Tok, Tok, "*")) {
        Ty = pointerTo(Ty);
    }

    if (Tok->Kind != TK_IDENT) {
        errorTok(Tok, "expected a variable name");
    }

    // typeSuffix
    Ty = typeSuffix(Rest, Tok->Next, Ty);
    // ident
    // 变量名 或 函数名
    Ty->Name = Tok;

    return Ty;
}

// declaration =
//     declspec (declarator ("=" expr)?("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **Rest, Token *Tok) {
    // declspec
    // 声明 基础类型
    Type *Basety = declspec(&Tok, Tok);

    Node Head = {};
    Node *Cur = &Head;
    // 对变量声明次数计数
    int I = 0;

    // (declarator ("=" expr()?("," declarator ("=" expr)?)*)?
    while (!equal(Tok, ";")) {
        // 第一个变量不必匹配 ";"
        if (I++ > 0) {
            Tok = skip(Tok, ",");
        }

        // declarator
        // 声明获取到变量类型，包括变量名
        Type *Ty = declarator(&Tok, Tok, Basety);
        Obj *Var = newLVar(getIdent(Ty->Name), Ty);

        // 如果不存在"="则为变量声明，不需要生成节点，已经存储在Locals中了
        if (!equal(Tok, "=")) {
            continue;
        }

        // 解析"="后面的Token
        Node *LHS = newVarNode(Var, Ty->Name);
        // 解析递归赋值语句
        Node *RHS = assign(&Tok, Tok->Next);
        Node *Node = newBinary(ND_ASSIGN, LHS, RHS, Tok);

        // 存放在表达语句中
        Cur->Next = newUnary(ND_EXPR_STMT, Node, Tok);
        Cur = Cur->Next;
    }

    // 将所有表达式语句存放在代码块中
    Node *Nd = newNode(ND_BLOCK, Tok);
    Nd->Body = Head.Next;
    *Rest = Tok->Next;
    return Nd;
}

// 将形参添加到Locals
static void createParamLVars(Type *Param) {
    if (Param) {
        // 递归到形参最底部
        // 先将最底部的加入Locals中，之后的都逐个加入到顶部，保持顺序不变
        createParamLVars(Param->Next);
        // 添加到Locals
        newLVar(getIdent(Param->Name), Param);
    }
}

// functionDefinition = declspec declarator? ident "(" ")" "{" compoundStmt*
static Token *function(Token *Tok, Type *BaseTy) {
    // declarator? ident "(" ")"
    Type *Ty = declarator(&Tok, Tok, BaseTy);

    Obj *Fn = newGVar(getIdent(Ty->Name), Ty);
    Fn->isFunction = true;

    // 清空全局变量 Locals
    Locals = NULL;

    // 进入新的域
    enterScope();
    // 函数参数
    createParamLVars(Ty->Params);
    Fn->Params = Locals;

    Tok = skip(Tok, "{");
    // 函数体存储语句的AST, Locals存储变量
    Fn->Body = compondStmt(&Tok, Tok);
    Fn->Locals = Locals;
    // 结束当前域
    leaveScope();
    return Tok;
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "for" "(" exprStmt expr?";" expr?";" expr? ")" stmt
//      | "while" "(" expr")" stmt
//      | "{" compondStmt 
//      | exprStmt
static Node *stmt(Token **Rest, Token *Tok) {
    if (equal(Tok, "return")) {
        Node *Nd = newNode(ND_RETURN, Tok);
        Nd->LHS = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ";");
        return Nd;
    }

    if (equal(Tok, "if")) {
        Node *Nd = newNode(ND_IF, Tok);
        Tok = skip(Tok->Next, "(");
        Nd->Cond = expr(&Tok, Tok);
        Tok = skip(Tok, ")");
        Nd->Then = stmt(&Tok, Tok);
        if (equal(Tok, "else")) {
            Nd->Els =stmt(&Tok, Tok->Next);
        }
        *Rest = Tok;
        return Nd;
    }

    // "for" "(" exprStmt expr?";" expr?";" expr? ")" stmt
    if (equal(Tok, "for")) {
        Node *Nd = newNode(ND_FOR, Tok);
        Tok = skip(Tok->Next, "(");
        
        // exprStm
        Nd->Init = exprStmt(&Tok, Tok);

        // expr?
        if (!equal(Tok, ";"))
            Nd->Cond = expr(&Tok, Tok);

        Tok = skip(Tok, ";");

        // expr?
        if (!equal(Tok, ")")) {
            Nd->Inc = expr(&Tok, Tok);
        }    

        Tok = skip(Tok, ")");

        Nd->Then = stmt(Rest, Tok);    

        return Nd;
    }

    // "while" "(" expr")" stmtd
    if (equal(Tok, "while")) {
        Node *Nd = newNode(ND_FOR, Tok);
        Tok = skip(Tok->Next, "(");
        Nd->Cond = expr(&Tok, Tok);
        Tok = skip(Tok, ")");
        Nd->Then = stmt(Rest, Tok);
        return Nd;
    }

    if (equal(Tok, "{")) {
        return compondStmt(Rest, Tok->Next);
    }

    return exprStmt(Rest, Tok);
}

// exprStmt = expr? ";"
static Node *exprStmt(Token **Rest, Token *Tok) {
    if (equal(Tok, ";")) {
        *Rest = Tok->Next;
        return newNode(ND_BLOCK, Tok);
    }
    Node *Nd = newNode(ND_EXPR_STMT, Tok);
    Nd->LHS = expr(&Tok, Tok);
    *Rest = skip(Tok, ";");    
    return Nd;
}

// expr = assign ("," expr)?
static Node *expr(Token **Rest, Token *Tok) {
    
    Node *Nd = assign(&Tok, Tok);

    if (equal(Tok, ",")) {
        return newBinary(ND_COMMA, Nd, expr(Rest, Tok->Next), Tok);
    }

    *Rest = Tok;

    return Nd;
}

// assign = equality ("=" assign)?
static Node *assign(Token **Rest, Token *Tok) {
    Node *Nd = equality(&Tok, Tok);

    if (equal(Tok, "=")) {
        Nd = newBinary(ND_ASSIGN, Nd, assign(&Tok, Tok->Next), Tok);
    }
    *Rest = Tok;
    return Nd;
    
}

// equality = relational("==" relational | "!=" relational)
static Node *equality(Token **Rest, Token *Tok) {
    Node *Nd = relational(&Tok, Tok);

    while (true) {
        Token *Start = Tok;

        if (equal(Tok, "==")) {
            Nd = newBinary(ND_EQ, Nd, relational(&Tok, Tok->Next), Start);
            continue;            
        }

        if (equal(Tok, "!=")) {
            Nd = newBinary(ND_NE, Nd, relational(&Tok, Tok->Next), Start);
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
        Token *Start = Tok;
        if (equal(Tok, "<=")) {
            Nd = newBinary(ND_LE, Nd, add(&Tok, Tok->Next), Start);
            continue;
        }

        if (equal(Tok, "<")) {
            Nd = newBinary(ND_LT, Nd, add(&Tok, Tok->Next), Start);
            continue;
        }

        // x >= y
        // y <= x
        if (equal(Tok, ">=")) {
            Nd = newBinary(ND_LE, add(&Tok, Tok->Next), Nd, Start);
            continue;
        }

        if (equal(Tok, ">")) {
            Nd = newBinary(ND_LT, add(&Tok, Tok->Next), Nd, Start);
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
        Token *Start = Tok;
        if (equal(Tok, "+"))
        {
            Nd = newAdd(Nd, mul(&Tok, Tok->Next), Start);
            continue;
        }

        if (equal(Tok, "-"))
        {
            Nd = newSub(Nd, mul(&Tok, Tok->Next), Tok);
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
        Token *Start = Tok;
        if (equal(Tok, "*"))
        {
            Nd = newBinary(ND_MUL, Nd, unary(&Tok, Tok->Next), Start);
            continue;
        }

        if (equal(Tok, "/"))
        {
            Nd = newBinary(ND_DIV, Nd, unary(&Tok, Tok->Next), Start);
            continue;
        }

        *Rest = Tok;
        return Nd;
    }
}

// 解析一元运算
// unary = unary("+" | "-" | "*" | "&")unary | postfix
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
        return newUnary(ND_NEG, unary(Rest, Tok->Next), Tok);
    }

    if (equal(Tok, "*")) {
        return newUnary(ND_DEREF, unary(Rest, Tok->Next), Tok);
    }

    if (equal(Tok, "&")) {
        return newUnary(ND_ADDR, unary(Rest, Tok->Next), Tok);
    }

    // primary
    return postfix(Rest, Tok);
}

// structMembers = (declspec declarator ("," declarator)* ";")*
static void structMembers(Token **Rest, Token *Tok, Type *Ty) {
    Member Head = {};
    Member *Cur = &Head;

    while (!equal(Tok, "}")) {
        //declspec
        int I = 0;
        Type *BaseTy = declspec(&Tok, Tok);

        while (!consume(&Tok, Tok, ";")) {
            if (I++) {
                Tok = skip(Tok, ",");
            }

            Member *Mem = calloc(1, sizeof(Member));
            // declarator
            Mem->Ty = declarator(&Tok, Tok, BaseTy);
            Mem->Name = Mem->Ty->Name;

            Cur = Cur->Next = Mem;
        }
    }

    *Rest = Tok->Next;
    Ty->Mems = Head.Next;
}

// structDecl = ident? ("{" structMembers)?
static Type *structUnionDecl(Token **Rest, Token *Tok) {
    // 读取标签
    Token *Tag = NULL;
    if (Tok->Kind == TK_IDENT) {
        Tag = Tok;
        Tok = Tok->Next;
    }

    // 如果标签存在，且后面不是{,就是返回这个Tag作为类型
    if (Tag && !equal(Tok, "{")) {
        Type *Ty = findTag(Tag);
        if (!Ty) {
            errorTok(Tok, "unkown struct type");
        }
        *Rest = Tok;
        return Ty;
    }

    // 构造一个结构体
    Type *Ty = calloc(1, sizeof(Type));
    Ty->Kind = TY_STRUCT;
    structMembers(Rest, Tok->Next, Ty);
    Ty->Align = 1;

    // 如果有名称就注册结构体类型
    if (Tag) {
        pushTagScope(Tag, Ty);
    }
    return Ty;
}

// structDecl = structUnionDecl
static Type *structDecl(Token **Rest, Token *Tok) 
{
    Type *Ty = structUnionDecl(Rest, Tok);
    Ty->Kind = TY_STRUCT;

    // 计算结构体内成员的偏移量
    int Offset = 0;
    for (Member *Mem = Ty->Mems; Mem; Mem = Mem->Next) {
        // 对齐成员的偏移量
        Offset = alignTo(Offset, Mem->Ty->Align);
        Mem->Offset = Offset;
        Offset += Mem->Ty->Size;

        // 偏移量为结构体成员的最大偏移量
        if (Ty->Align < Mem->Ty->Align)
            Ty->Align = Mem->Ty->Align;
    }
    // 对齐结构体的偏移量
    Ty->Size = alignTo(Offset, Ty->Align);
    //Ty->Size = Offset;

    return Ty;
}

// unionDecl = structUnionDecl
static Type *unionDecl(Token **Rest, Token *Tok) {
    Type *Ty = structUnionDecl(Rest, Tok);
    Ty->Kind = TY_UNION;

    // 联合体需要设置为最大的对齐量与大小，变量偏移量都默认为0
    for (Member *Mem = Ty->Mems; Mem; Mem=Mem->Next) {
        if (Mem->Ty->Align > Ty->Align) {
            Ty->Align = Mem->Ty->Align  ;
        }

        if (Mem->Ty->Size > Ty->Size) {
            Ty->Size = Mem->Ty->Size;
        }
    }
    // 将大小对齐
    Ty->Size = alignTo(Ty->Size, Ty->Align);
    return Ty;
}

// 获取结构体成员
static Member *getStructMember(Type *Ty, Token *Tok) {
    for (Member *Mem = Ty->Mems; Mem; Mem = Mem->Next) {
        if (Mem->Name->Len == Tok->Len &&
            !strncmp(Mem->Name->Loc, Tok->Loc, Tok->Len)) {
                return Mem;
            }
    }

    errorTok(Tok, "No such member");
    return NULL;
}

// 构建结构体成员的节点
static Node *structRef(Node *LHS, Token *Tok) {
    addType(LHS);

    if (LHS->Ty->Kind != TY_STRUCT && LHS->Ty->Kind != TY_UNION) {
        errorTok(LHS->Tok, "not a struct nor a union");
    }

    Node *Nd = newUnary(ND_MEMBER, LHS, Tok);
    Nd->Mem = getStructMember(LHS->Ty, Tok);
    return Nd;
}

// postfix = primary ("[" expr "]" | "." ident)* | "->" ident)*
static Node *postfix(Token **Rest, Token *Tok) {
    // primary
    Node *Nd = primary(&Tok, Tok);

    // ("[" expr "]")
    while (true) {
        if (equal(Tok, "[")) {
            // x[y] 等价于*(x+y)
            Token *Start = Tok;
            Node *Idx = expr(&Tok, Tok->Next);
            Tok = skip(Tok, "]");
            Nd = newUnary(ND_DEREF, newAdd(Nd, Idx, Start), Start);
            continue;
        }

        // "." ident
        if (equal(Tok, ".")) {
            Nd = structRef(Nd, Tok->Next);
            Tok = Tok->Next->Next;
            continue;
        }

        // "->" ident
        if (equal(Tok, "->")) {
            // x->y 等价于 (*x).y
            Nd = newUnary(ND_DEREF, Nd, Tok);
            Nd = structRef(Nd, Tok->Next);
            Tok = Tok->Next->Next;
            continue;
        }
        
        *Rest = Tok;
        return Nd;
    }
    
}

// 解析括号、数字、变量
// primary = "(" "{" stmt+ "}"")" 
//          |"("expr")" 
//          | "sizeof" unary  
//          | ident args? 
//          | str 
//          | num
static Node *primary(Token **Rest, Token *Tok)
{
    // "(" "{" stmt+ "}" ")"
    if (equal(Tok, "(") && equal(Tok->Next, "{")) {
        // This is a GNU statement expression.  
        Node *Nd = newNode(ND_STMT_EXPR, Tok);
        Nd->Body = compondStmt(&Tok, Tok->Next->Next)->Body;
        *Rest = skip(Tok, ")");
        return Nd;
    }
        
    // "(" expr ")"
    if (equal(Tok, "("))
    {
        Node *Nd = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ")");
        return Nd;
    }

    // "sizeof" unary
    if (equal(Tok, "sizeof")) {
        //Node *Nd = expr(Rest, Tok->Next);
        Node *Nd = unary(Rest, Tok->Next);
        addType(Nd);
        return newNum(Nd->Ty->Size, Tok);
    }

    // ident args?
    if (Tok->Kind == TK_IDENT) {
        if (equal(Tok->Next, "(")) { 
            return funCall(Rest, Tok);
        }

        // ident
        // 查找变量
        Obj *Var =findVar(Tok);
        if (!Var) {
           errorTok(Tok, "undefined variable");
        }
        
        *Rest = Tok->Next;
        return newVarNode(Var, Tok);
    }

    // str
    if (Tok->Kind == TK_STR) {
        Obj *Var = newStringLiteral(Tok->Str, Tok->Ty);
        *Rest = Tok->Next;
        return newVarNode(Var, Tok);
    }

    // num
    if (Tok->Kind == TK_NUM)
    {
        Node *Nd = newNum(Tok->Val, Tok);
        *Rest = Tok->Next;
        return Nd;
    }

    errorTok(Tok, "expected an expression");
    return NULL;
}

// funcall = ident "(" (assign ("," assign)*)? ")"
static Node *funCall(Token **Rest, Token *Tok) {
    Token *Start = Tok;
    Tok = Tok->Next->Next;

    Node Head = {};
    Node *Cur = &Head;

    while (!equal(Tok, ")")) {
        if (Cur != &Head) {
            Tok = skip(Tok, ",");
        }
        // assign
        Cur->Next = assign(&Tok, Tok);
        Cur = Cur->Next;
    }

    *Rest = skip(Tok, ")");

    Node *Nd = newNode(ND_FUNCALL, Start);
    // ident
    Nd->FuncName = strndup(Start->Loc, Start->Len);
    Nd->Args = Head.Next;
    return Nd;
}

// 构造全局变量
static Token *globalVariable(Token *Tok, Type *Basety) {
    bool First = true;

    while (!consume(&Tok, Tok, ";")) {
        if (!First) {
            Tok = skip(Tok, ",");
        }
        First = false;
        
        Type *Ty = declarator(&Tok, Tok, Basety);
        newGVar(getIdent(Ty->Name), Ty);
    }

    return Tok;
}

// 区分 函数还是全局变量
static bool isFunction(Token *Tok) {
    if (equal(Tok, ";")) {
        return false;
    }

    // 虚设变量，用于调用declarator
    Type Dummy = {};
    Type *Ty = declarator(&Tok, Tok, &Dummy);
    return Ty->Kind == TY_FUNC;
}

// 语法解析入口函数
// program = (functionDefinition | global-variable)*
Obj *parse(Token *Tok) {
     Globals = NULL;

     while (Tok->Kind != TK_EOF) {
        Type *BaseTy = declspec(&Tok, Tok);

        // 函数
        if (isFunction(Tok)) {
            Tok = function(Tok, BaseTy);
            continue;
        }

        // 全局变量
        Tok = globalVariable(Tok, BaseTy);
     }

     return Globals;
}