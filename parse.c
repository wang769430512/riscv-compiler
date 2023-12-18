#include "rvcc.h"

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

Node *parse(Token **Rest, Token *Tok) {
    Node *Nd = expr(&Tok, Tok);

    if (Tok->Kind != TK_EOF)
    {
        errorTok(Tok, "extra token");
    }

    return Nd;
}

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

