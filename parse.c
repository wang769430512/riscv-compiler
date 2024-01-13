#include "rvcc.h"

Obj* Locals;

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

    // ptr + num ptr+1这里的1是一个元素
    RHS = newBinary(ND_MUL, RHS, newNum(8, Tok), Tok);

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
        RHS = newBinary(ND_MUL, RHS, newNum(8, Tok), Tok);
        addType(RHS);
        Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
        Nd->Ty = LHS->Ty;
        return Nd;
    }
    
    // ptr - ptr 
    if (LHS->Ty->Base && RHS->Ty->Base) {
        Node *Nd = newBinary(ND_SUB, LHS, RHS, Tok);
        Nd->Ty = TyInt;
        return newBinary(ND_DIV, Nd, newNum(8, Tok), Tok);
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

static Obj *newLVar(char *Name, Type *Ty) {
    Obj *Var = calloc(1, sizeof(Obj));
    Var->Name = Name;
    Var->Next = Locals;
    Var->Ty = Ty;
    // 将变量插入头部
    Var->Next = Locals;
    Locals = Var;
    
    return Var;
}

static Node *newVarNode(Obj *Var, Token *Tok) {
    Node *Nd = newNode(ND_VAR, Tok);
    Nd->Var = Var;
    return Nd;
}

static Obj *findVar(Token *Tok) {
    for (Obj *Var=Locals; Var; Var=Var->Next) {
        if (strlen(Var->Name) == Tok->Len && (!strncmp(Var->Name, Tok->Loc, Tok->Len))) {
            return Var;
        } 
    }

    return NULL;
}

// program = "{" compoundStmt
// compondStmt = stmt* "}"
// stmt =  ""return" expr ";"
//        | "if" "(" expr ")" stmt ("else" stmt)? 
//        | "for" "(" exprStmt expr?";" expr? ";" expr? ";")" stmt
//        | "while" "(" expr ")" stmt
//        | "{" compondStmt 
//        | exprStmt
// exprStmt = expr? ";"
// expr = assign
// assign = equality ("=" assign)?
// equality = relational("==" relational | "!=" relational )*
// relational = add("<=" add | "<" add | ">=" add | ">" add)*
// add = mul("+"mul | "-"mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" | "*" | "/" | "*" | "&") unary | primary
// primary = "("expr")" | ident | num
//static Node *program(Token **Rest, Token *Tok);
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
static Node *unary(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);

// compondStmt = (declaration | stmt)* "}"
static Node *compondStmt(Token **Rest, Token *Tok) {
    Node *Nd = newNode(ND_BLOCK, Tok);

    Node Head = {};
    Node *Cur = &Head;

    while (!equal(Tok, "}")) {
        
        if (equal(Tok, "int")) {
            Cur->Next = declaration(&Tok, Tok);
        } else {
            Cur->Next = stmt(&Tok, Tok);
        }
        Cur = Cur->Next;
        // 构造完AST之后，添加节点类型
        addType(Cur);
    } 

    Nd->Body = Head.Next;
    *Rest = Tok->Next;

    return Nd;
}

// 获取标识符
static char *getIdent(Token *Tok) {
    if (Tok->Kind != TK_IDENT) 
        errorTok(Tok, "expected an identifier");

    return strndup(Tok->Loc, Tok->Len);
}

// declspec = "int"
// declarator specifier
static Type *declspec(Token **Rest, Token *Tok) {
    *Rest = skip(Tok, "int");
    return TyInt;
}

// declarator = "*"* ident
static Type* declarator(Token **Rest, Token* Tok, Type *Ty) {
    // "*"*
    // 构建所有的(多重)指针
    while (consume(&Tok, Tok, "*")) {
        Ty = pointerTo(Ty);
    }

    if (Tok->Kind != TK_IDENT) {
        errorTok(Tok, "expected a variable name");
    }

    // ident
    // 变量名
    Ty->Name = Tok;
    *Rest = Tok->Next;
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

// expr = equality
static Node *expr(Token **Rest, Token *Tok) {
    return assign(Rest, Tok);
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
        // x < y
        if (equal(Tok, ">=")) {
            Nd = newBinary(ND_LT, add(&Tok, Tok->Next), Nd, Start);
            continue;
        }

        if (equal(Tok, ">")) {
            Nd = newBinary(ND_LE, add(&Tok, Tok->Next), Nd, Start);
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

// unary = unary("+" | "-" | "*" | "&")unary | primary
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
    return primary(Rest, Tok);
}

// primary = "("expr")" | ident | num
static Node *primary(Token **Rest, Token *Tok)
{
    if (equal(Tok, "("))
    {
        Node *Nd = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ")");
        return Nd;
    }

    if (Tok->Kind == TK_IDENT) {
        Obj *Var =findVar(Tok);
        if (!Var) {
           errorTok(Tok, "undefined variable");
        }
        
        *Rest = Tok->Next;
        return newVarNode(Var, Tok);
    }

    if (Tok->Kind == TK_NUM)
    {
        Node *Nd = newNum(Tok->Val, Tok);
        *Rest = Tok->Next;
        return Nd;
    }

    errorTok(Tok, "expected an expression");
    return NULL;
}

// program = "{" CompondStmt
Function *parse(Token *Tok) {

    Tok = skip(Tok, "{");

    Function *prog = calloc(1, sizeof(Function));;
    prog->Body = compondStmt(&Tok, Tok);
    prog->Locals = Locals;

    return prog;
}