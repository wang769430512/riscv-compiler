#include "rvcc.h"

Obj* Locals;

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

static Obj *newLVar(char *Name) {
    Obj *Var = calloc(1, sizeof(Obj));
    Var->Name = Name;
    Var->Next = Locals;
    Locals = Var;
    
    return Var;
}

static Node *newVarNode(Obj *Var) {
    Node *Nd = newNode(ND_VAR);
    Nd->Name = Var;
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
// unary = ("+" | "-") unary | primary
// primary = "("expr")" | ident | num
static Node *program(Token **Rest, Token *Tok);
static Node *compondStmt(Token **Rest, Token *Tok);
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

// compondStmt = stmt* "}"
static Node *compondStmt(Token **Rest, Token *Tok) {
    Node Head = {};
    Node *Cur = &Head;

    while (!equal(Tok, "}")) {
        Cur->Next = stmt(&Tok, Tok);       
        Cur = Cur->Next;
    } 

    Node *Nd = newNode(ND_BLOCK);
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
        Node *Nd = newUnary(ND_RETURN, expr(&Tok, Tok->Next));
        *Rest = skip(Tok, ";");
        return Nd;
    }

    if (equal(Tok, "if")) {
        Node *Nd = newNode(ND_IF);
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
        Node *Nd = newNode(ND_FOR);
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
        Node *Nd = newNode(ND_FOR);
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
        return newNode(ND_BLOCK);
    }
    Node *Nd = newUnary(ND_EXPR_STMT, expr(&Tok, Tok));
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
        Nd = newBinary(ND_ASSIGN, Nd, assign(&Tok, Tok->Next));
    }
    *Rest = Tok;
    return Nd;
    
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
            Var =newLVar(strndup(Tok->Loc, Tok->Len));
        }
        
        *Rest = Tok->Next;
        return newVarNode(Var);
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

// program = "{" CompondStmt
Function *parse(Token *Tok) {

    Tok = skip(Tok, "{");

    Function *prog = calloc(1, sizeof(Function));;
    prog->Body = compondStmt(&Tok, Tok);
    prog->Locals = Locals;

    return prog;
}