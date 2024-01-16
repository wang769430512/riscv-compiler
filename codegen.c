#include "rvcc.h"

// 记录栈深度
static int Depth;
// 用于函数参数的寄存器们
static char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
// 当前的函数
static Function *CurrentFn;

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
        // 偏移量是相对于fp的
        printf("  # 获取变量%s的栈内存地址为%d(fp)\n", Nd->Var->Name, Nd->Var->Offset);
        printf("  addi a0, fp, %d\n", Nd->Var->Offset);
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
    // 函数调用
    case ND_FUNCALL: {
        // 记录参数个数
        int NArgs = 0;
        // 计算所有参数的值，正向压栈
        for (Node *Arg = Nd->Args; Arg; Arg = Arg->Next) {
            genExpr(Arg);
            push();
            NArgs++;
        }

        // 反向弹栈，a0->参数1, a1->参数2 ...
        for (int i = NArgs - 1; i >= 0; i--) {
            pop(ArgReg[i]);
        }

        // 调用函数
        printf("  # 调用函数%s\n", Nd->FuncName);
        printf("  call %s\n", Nd->FuncName);
        return;
    }
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
        // 生成return语句
        case ND_RETURN:
            printf("# 返回语句\n");
            genExpr(Nd->LHS);
            // 无条件跳转语句，跳转到.L.return段
            // j offset是 jal x0, offset的别名指令
            printf("  # 跳转到.L.return.%s段\n", CurrentFn->Name);
            printf("  j .L.return.%s\n", CurrentFn->Name);
            return;
        case ND_EXPR_STMT:
            genExpr(Nd->LHS);
            return;
        default:
            break;
    }

    errorTok(Nd->Tok, "Invalid statement");
}

static void assignLVarOffsets(Function* Prog) {
    // 为每个函数计算其变量所用的栈空间
    for (Function* Fn = Prog; Fn; Fn = Fn->Next) {
        int Offset = 0;
        // 读取所有变量
        for (Obj* Var = Fn->Locals; Var; Var = Var->Next) {
            // 每个变量分配8字节
            Offset += 8;
            // 为每个变量赋一个偏移量，或者说是栈中地址
            Var->Offset = -Offset;
        }

        // 将栈对齐到16字节
        Fn->StackSize = alignTo(Offset, 16);
    }
}

void codegen(Function* Prog) {
    assignLVarOffsets(Prog);

    // 为每个函数单独生成代码
    for (Function *Fn = Prog; Fn; Fn = Fn->Next) {
        printf("\n  # 定义全局 %s\n", Fn->Name);
        printf("  .globl %s\n", Fn->Name);
        printf("# =====%s段开始===============\n", Fn->Name);
        printf("# %s段标签\n", Fn->Name);
        printf("%s:\n", Fn->Name);
        CurrentFn = Fn;

        // 栈布局
        //------------------------------------------//sp
        //                    ra
        //------------------------------------------// ra = sp - 8
        //                    fp                     
        //------------------------------------------//fp = sp - 16
        //                   变量
        //-----------------------------------------//sp = sp - 16 - stackSize
        //                 表达式计算
        //-----------------------------------------//

        // Prologue, 前言
        // 将ra寄存器压入栈中，保持ra的值
        printf("  # 将ra寄存器压栈,保存ra的值\n");
        printf("  addi sp, sp, -16\n");
        printf("  sd ra, 8(sp)\n");
        // 将fp压入栈中，保存fp的值
        printf("  # 将fp压栈, fp属于\"被调用者保存\"的寄存器,需要恢复原值\n");
        printf("  sd fp, 0(sp)\n");
        // 将sp写入fp
        printf("  # 将sp的值写入fp\n");
        printf("  mv fp, sp\n");

        // 偏移量为实际变量所用的栈大小
        printf("  # sp腾出StackSize大小的栈空间\n");
        printf("  addi sp, sp, -%d\n", Fn->StackSize);

        // 生成语句链表的代码
        printf("\n# ===== %s段主体 ============\n", Fn->Name);
        genStmt(Fn->Body);
        assert(Depth == 0);

        // Epilogue,后语
        // 输出return标签
        printf("\n# ===== %s段结束 ============\n", Fn->Name);
        printf("# return段标签\n");
        printf(".L.return.%s:\n", Fn->Name);
        // 将fp的值改写回sp
        printf("  mv sp, fp\n");
        // 将最早fp保存的值弹栈，恢复fp
        printf("  # 将最早fp保存的值弹栈,恢复fp和sp\n");
        printf("  ld fp, 0(sp)\n");
        // 将ra寄存器弹栈,恢复ra的值
        printf("  # 将ra寄存器弹栈,恢复ra的值\n");
        printf("  ld ra, 8(sp)\n");   
        printf("  addi sp, sp, 16\n");
    
        // 返回
        printf("  # 返回a0值给系统调用\n");
        printf("  ret\n");
    }  
}
