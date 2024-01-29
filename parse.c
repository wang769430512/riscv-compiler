#include "rvcc.h"

// 在解析时，全部的变量实例都被累加到这个列表里
Obj* Locals;  // 局部变量
Obj* Globals; // 全局变量

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

// 新建变量
static Obj *newVar(char *Name, Type *Ty) {
    Obj *Var = calloc(1, sizeof(Obj));
    Var->Name = Name;
    Var->Ty = Ty;

    return Var;
}

static Obj *newLVar(char *Name, Type *Ty) {
    Obj *Var = newVar(Name, Ty);
    Var->isLocal = true;
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

static Obj *findVar(Token *Tok) {
    for (Obj *Var=Locals; Var; Var=Var->Next) {
        if (strlen(Var->Name) == Tok->Len && (!strncmp(Var->Name, Tok->Loc, Tok->Len))) {
            return Var;
        } 
    }

    return NULL;
}

// program = functionDefinition*
// functionDefinition = declspec declarator "{" compoundStmt*
// declspec = "int"
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
// expr = assign
// assign = equality ("=" assign)?
// equality = relational("==" relational | "!=" relational )*
// relational = add("<=" add | "<" add | ">=" add | ">" add)*
// add = mul("+"mul | "-"mul)*
// mul = unary ("*" unary | "/" unary)*
// unary = ("+" | "-" | "*" | "/" | "*" | "&") unary | primary
// postfix = primary ("[" expr "]")*
// primary = "(" expr ")" | "sizeof" unary | ident func-args? | num

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
static Node *unary(Token **Rest, Token *Tok);
static Node *postfix(Token **Rest, Token *Tok);
static Node *primary(Token **Rest, Token *Tok);
static Node *funCall(Token **Rest, Token *Tok);

// compondStmt = (declaration | stmt)* "}"
static Node *compondStmt(Token **Rest, Token *Tok) {
    Node *Nd = newNode(ND_BLOCK, Tok);
    
    // 这里使用了和词法分析类似的单向链表结构
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

// 获取数字
static int getNumber(Token *Tok) {
    if (Tok->Kind != TK_NUM) {
        errorTok(Tok, "expected a number");
    }
    return Tok->Val;
}

// declspec = "int"
// declarator specifier
static Type *declspec(Token **Rest, Token *Tok) {
    *Rest = skip(Tok, "int");
    return TyInt;
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

    // 函数参数
    createParamLVars(Ty->Params);
    Fn->Params = Locals;

    Tok = skip(Tok, "{");
    // 函数体存储语句的AST, Locals存储变量
    Fn->Body = compondStmt(&Tok, Tok);
    Fn->Locals = Locals;
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

// postfix = primary ("[" expr "]")*
static Node *postfix(Token **Rest, Token *Tok) {
    // primary
    Node *Nd = primary(&Tok, Tok);

    // ("[" expr "]")
    while (equal(Tok, "[")) {
        // x[y] 等价于*(x+y)
        Token *Start = Tok;
        Node *Idx = expr(&Tok, Tok->Next);
        Tok = skip(Tok, "]");
        Nd = newUnary(ND_DEREF, newAdd(Nd, Idx, Tok), Start);
    }
    *Rest = Tok;
    return Nd;
}

// primary = "("expr")" | ident args? | num
static Node *primary(Token **Rest, Token *Tok)
{
    if (equal(Tok, "("))
    {
        Node *Nd = expr(&Tok, Tok->Next);
        *Rest = skip(Tok, ")");
        return Nd;
    }

    // "sizeof" unary
    if (equal(Tok, "sizeof")) {
        Node *Nd = expr(Rest, Tok->Next);
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

// 语法解析入口函数
// program = (functionDefinition | global-variable)*
Obj *parse(Token *Tok) {
     Globals = NULL;

     while (Tok->Kind != TK_EOF) {
        Type *BaseTy = declspec(&Tok, Tok);
        Tok = function(Tok, BaseTy);
     }

     return Globals;
}