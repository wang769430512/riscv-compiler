#include "rvcc.h"

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

static void genAddr(Node *Nd) {
    if (Nd->Kind == ND_VAR) {
        int offset = (Nd->Name - 'a' + 1) * 8;
        printf("  addi a0, fp, %d\n", -offset);
        return;
    }

    error("not an lvalue");
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
     case ND_VAR:
        genAddr(Nd);
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_ASSIGN:
        genAddr(Nd->LHS);
        push();
        genExpr(Nd->RHS);
        pop("a1");
        printf("  sd a0, 0(a1)\n");
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

static void genStmt(Node *Nd) {
    if (Nd->Kind == ND_EXPR_STMT) {
        genExpr(Nd->LHS);
        return;
    }

    error("Invalid statement");
}

void codegen(Node* Nd) {
    printf("  .globl main\n");
    printf("main:\n");

    // 栈布局
    //------------------------------------------//sp
    //                    fp                      fp = sp - 8
    //------------------------------------------//fp
    //                    'a'                     fp - 8
    //                    'b'                     fp - 16
    //                    ...
    //                    'z'                     fp - 208
    //-----------------------------------------//sp = sp - 8 - 216
    //                 表达式计算
    //-----------------------------------------

    // Prologue, 前言
    // 将fp压入栈中，保存fp的值
    printf("  addi sp, sp, -8\n");
    printf("  sd fp, 0(sp)\n");

    printf("  mv fp, sp\n");

    printf("  addi sp, sp, -208\n");
    // 遍历AST树生成汇编
    for (Node *N = Nd; N; N=N->Next) {
        genStmt(N);
         // 如果栈未清空，则报错
        assert(Depth == 0);
    }

    //printf("  addi sp, sp, 208\n"); 
    printf("  mv sp, fp\n");
    printf("  ld fp, 0(sp)\n");   
    printf("  addi sp, sp, 8\n");
       
    printf("  ret\n");
}
