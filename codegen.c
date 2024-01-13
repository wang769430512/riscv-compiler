#include "rvcc.h"

static int Depth;

static void genExpr(Node *Nd);

static int count(void) {
    static int I = 1;
    return I++;
}

static void push(void)
{
    printf("  addi sp, sp, -8\n");
    printf("  sd a0, 0(sp)\n");
    Depth++;
}


static void pop(char *Reg)
{
    printf("  # 弹栈, 将栈顶的值存入%s\n", Reg);
    printf("  ld %s, 0(sp)\n", Reg);
    printf("  addi sp, sp, 8\n");
    Depth--;
}

static int alignTo(int N, int Allign) {
    return (N + Allign - 1) / Allign * Allign;
}

static void genAddr(Node *Nd) {
    switch(Nd->Kind) {
    case ND_VAR:
        printf("  # 获取变量%s的栈内存地址为%d(fp)\n", Nd->Var->Name, Nd->Var->offset);
        printf("  addi a0, fp, %d\n", Nd->Var->offset);
        return;
    // 解引用*
    case ND_DEREF:
        genExpr(Nd->LHS);
        return;
    default:
        break;
    }

    errorTok(Nd->Tok, "not an lvalue");
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
        // 计算出变量的地址，然后存入a0
        genAddr(Nd);
        // 访问a0地址中存储的数据，存入到a0当中
        printf("  # 读取a0中存放的地址,得到的值存放到a0中\n");
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_ADDR:
        genAddr(Nd->LHS);
        return;
    case ND_DEREF:
        genExpr(Nd->LHS);
        printf("  # 读取a0中存放的地址,得到的值存放到a0中\n");
        printf("  ld a0, 0(a0)\n");
        return;
    case ND_ASSIGN:
        // 左部是左值，保存值的地址
        genAddr(Nd->LHS);
        push();
        // 右部是右值，为表达式的值
        genExpr(Nd->RHS);
        pop("a1");
        printf("  # 将a0的值,写入到a1中存放的地址\n");
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
        printf("  # a0+a1, 结果写入a0\n");
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

    errorTok(Nd->Tok, "invalid expression");
}

static void genStmt(Node *Nd) {
    switch(Nd->Kind) {
        case ND_IF: {
            // 代码段计数
            int C = count();
            genExpr(Nd->Cond);
            printf("  beqz a0, .L.else.%d\n", C);
            genStmt(Nd->Then);
            printf("  j .L.end.%d\n", C);
            printf(".L.else.%d:\n", C);
            if (Nd->Els) {
                genStmt(Nd->Els);
                printf(".L.end.%d:\n", C);
            }
            return;
        }
        case ND_FOR: {
            int C = count();
            if (Nd->Init) {
                genStmt(Nd->Init);
            }
            printf(".L.begin.%d:\n", C);
            if (Nd->Cond) {
                genExpr(Nd->Cond);
                printf("  beqz a0, .L.end.%d\n", C);
            }  
            genStmt(Nd->Then);
            if (Nd->Inc) {
                genExpr(Nd->Inc);  
            }
            printf("  j .L.begin.%d\n", C);
            printf(".L.end.%d:\n", C);
        
            return;
        }
        case ND_BLOCK:
            for (Node *N = Nd->Body; N; N=N->Next) {
                genStmt(N);
            }
            return;
        case ND_RETURN:
            printf("# 返回语句\n");
            genExpr(Nd->LHS);
            printf("  j .L.return\n");
            return;
        case ND_EXPR_STMT:
            genExpr(Nd->LHS);
            return;
        default:
            break;
    }

    errorTok(Nd->Tok, "Invalid statement");
}

static void assignLVarOffsets(Function* proc) {
    int offset = 0;
    for (Obj* Var = proc->Locals; Var; Var = Var->Next) {
        offset += 8;
        Var->offset = -offset;
    }

    proc->StackSize = alignTo(offset, 16);
}

void codegen(Function* Prog) {
    assignLVarOffsets(Prog);
    printf("  .globl main\n");
    printf("main:\n");

    // 栈布局
    //------------------------------------------//sp
    //                    fp                      fp = sp - 8
    //------------------------------------------//fp
    //                   变量
    //-----------------------------------------//sp = sp - 8 - stackSize
    //                 表达式计算
    //-----------------------------------------//

    // Prologue, 前言
    // 将fp压入栈中，保存fp的值
    printf("  addi sp, sp, -8\n");
    printf("  sd fp, 0(sp)\n");

    printf("  mv fp, sp\n");

    printf("  addi sp, sp, -%d\n", Prog->StackSize);
    genStmt(Prog->Body);
    assert(Depth == 0);

    printf(".L.return:\n");
    printf("  mv sp, fp\n");
    printf("  ld fp, 0(sp)\n");   
    printf("  addi sp, sp, 8\n");
       
    printf("  ret\n");
}
