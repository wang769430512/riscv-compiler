#include "rvcc.h"

// 记录栈深度
static int Depth;
// 用于函数参数的寄存器们
static char *ArgReg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
// 当前的函数
static Obj *CurrentFn;

static void genExpr(Node *Nd);
static void genStmt(Node *Nd);

static void printLn(char *Fmt, ...) {
    va_list VA;

    va_start(VA, Fmt);
    vprintf(Fmt, VA);
    va_end(VA);

    printf("\n");
}

static int count(void) {
    static int I = 1;
    return I++;
}

static void push(void)
{   
    printLn("  # 压栈,将a0的值存入栈顶");
    printLn("  addi sp, sp, -8");
    printLn("  sd a0, 0(sp)");
    Depth++;
}


static void pop(char *Reg)
{
    printLn("  # 弹栈, 将栈顶的值存入%s", Reg);
    printLn("  ld %s, 0(sp)", Reg);
    printLn("  addi sp, sp, 8");
    Depth--;
}

static int alignTo(int N, int Allign) {
    return (N + Allign - 1) / Allign * Allign;
}

static void genAddr(Node *Nd) {
    switch(Nd->Kind) {
    case ND_VAR:
        if (Nd->Var->IsLocal) { // 偏移量是相对于fp的
            printLn("  # 获取局部变量%s的栈内地址为%d(fp)", Nd->Var->Name, Nd->Var->Offset);
            printLn("  addi a0, fp, %d", Nd->Var->Offset);
        } else {
            printLn("  # 获取全局变量%s的地址", Nd->Var->Name);
            // 获取全局变量的地址
            // 高地址(高20位， 31~20位)
            printLn("  lui a0, %%hi(%s)", Nd->Var->Name);
            // 低地址(低12位, 19~0位)
            printLn("  addi a0, a0, %%lo(%s)", Nd->Var->Name);
            // printLn("  la a0, %s", Nd->Var->Name);
        }
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

// 加载a0指向的值
static void load(Type *Ty) {
    if (Ty->Kind == TY_ARRAY) {
        return;
    }

    printLn("  # 读取a0中存放的地址,得到的值存入a0");
    if (Ty->Size == 1) {
        printLn("  lb a0, 0(a0)");
    } else {
        printLn("  ld a0, 0(a0)");
    }
   
}

// 将栈顶值(为一个地址)存入a0
static void store(Type *Ty) {
    pop("a1");
    printLn("  # 将a0的值,写入到a1中存放的地址");
    if (Ty->Size == 1) {
        printLn("  sb a0, 0(a1)");
    } else {
        printLn("  sd a0, 0(a1)");
    }
}

static void genExpr(Node *Nd)
{
    switch (Nd->Kind) {
    case ND_NUM:
        printLn("  li a0, %d", Nd->Val);
        return;
    case ND_NEG: // -- a0 = -a1;
        genExpr(Nd->LHS);
        printLn("  neg a0, a0");
        return;
     case ND_VAR:
        // 计算出变量的地址，然后存入a0
        genAddr(Nd);
        load(Nd->Ty);
        return;
    case ND_ADDR:
        genAddr(Nd->LHS);
        return;
    case ND_DEREF:
        genExpr(Nd->LHS);
        load(Nd->Ty);
        return;
    case ND_ASSIGN:
        // 左部是左值，保存值的地址
        genAddr(Nd->LHS);
        push();
        // 右部是右值，为表达式的值
        genExpr(Nd->RHS);
        store(Nd->Ty);
        return;
    case ND_STMT_EXPR:
        for (Node *N = Nd->Body; N; N = N->Next) {
            genStmt(N);
        }
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
        printLn("  # 调用函数%s", Nd->FuncName);
        printLn("  call %s", Nd->FuncName);
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
        printLn("  # a0+a1, 结果写入a0");
        printLn("  add a0, a0, a1");
        return;
    case ND_SUB: // - a0 = a0 - a1;
        printLn("  sub a0, a0, a1");
        return;
    case ND_MUL: // * a0 = a0 * a1;
        printLn("  mul a0, a0, a1");
        return;
    case ND_DIV: // / a0 = a0 / a1;
        printLn("  div a0, a0, a1");
        return;
    case ND_EQ:
    case ND_NE:
        // a0 = a0 ^ a1
        printLn("  xor a0, a0, a1");
        if (Nd->Kind == ND_EQ) {
            printLn("  seqz a0, a0");
        } else {
            printLn("  snez a0, a0");
        }
        return;
    case ND_LT:
        printLn("  slt a0, a0, a1");
        return;
    case ND_LE:
        // a0 <= a1
        // a0=a1<a0, a0=a0^1
        printLn("  slt a0, a1, a0");
        printLn("  xori a0, a0, 1");
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
            printLn("  beqz a0, .L.else.%d", C);
            genStmt(Nd->Then);
            // 执行完后跳转到if语句后面的语句
            printLn("  j .L.end.%d", C);
            printLn(".L.else.%d:", C);
            if (Nd->Els) {
                genStmt(Nd->Els);                
            }
            printLn(".L.end.%d:", C);
            return;
        }
        case ND_FOR: {
            int C = count();
            if (Nd->Init) {
                genStmt(Nd->Init);
            }
            printLn(".L.begin.%d:", C);
            if (Nd->Cond) {
                genExpr(Nd->Cond);
                printLn("  beqz a0, .L.end.%d", C);
            }  
            genStmt(Nd->Then);
            if (Nd->Inc) {
                genExpr(Nd->Inc);  
            }
            printLn("  j .L.begin.%d", C);
            printLn(".L.end.%d:", C);
        
            return;
        }
        case ND_BLOCK:
            for (Node *N = Nd->Body; N; N=N->Next) {
                genStmt(N);
            }
            return;
        // 生成return语句
        case ND_RETURN:
            printLn("# 返回语句");
            genExpr(Nd->LHS);
            // 无条件跳转语句，跳转到.L.return段
            // j offset是 jal x0, offset的别名指令
            printLn("  # 跳转到.L.return.%s段", CurrentFn->Name);
            printLn("  j .L.return.%s", CurrentFn->Name);
            return;
        case ND_EXPR_STMT:
            genExpr(Nd->LHS);
            return;
        default:
            break;
    }

    errorTok(Nd->Tok, "Invalid statement");
}

static void assignLVarOffsets(Obj* Prog) {
    // 为每个函数计算其变量所用的栈空间
    for (Obj* Fn = Prog; Fn; Fn = Fn->Next) {
        // 如果不是函数，则终止
        if (!Fn->isFunction) {
            continue;
        }

        int Offset = 0;
        // 读取所有变量
        for (Obj* Var = Fn->Locals; Var; Var = Var->Next) {
            // 每个变量分配8字节
            Offset += Var->Ty->Size;
            // 为每个变量赋一个偏移量，或者说是栈中地址
            Var->Offset = -Offset;
        }

        // 将栈对齐到16字节
        Fn->StackSize = alignTo(Offset, 16);
    }
}

static void emitData(Obj *Prog) {
    for (Obj* Var=Prog; Var; Var=Var->Next) {
        if (Var->isFunction) {
            continue;
        }

        printLn("  # 数据标签开始");
        printLn("  .data");
        
       
        if (Var->InitData) {
            printLn("%s:", Var->Name);
            // 打印字符串的内容，包括转义字符
            for (int I = 0; I < Var->Ty->Size; I++) {
                char C = Var->InitData[I];
                if (isprint(C)) {
                    printLn("  .byte %d\t# 字符: %c", C, C);
                } else {
                    printLn("  .byte %d", C);
                }
            }
        } else {
            printLn("  # 全局段%s", Var->Name);
            printLn("  .global %s", Var->Name);
            printLn("%s:", Var->Name);
            printLn("  # 全局变量零填充%d位", Var->Ty->Size);
            printLn("  .zero %d", Var->Ty->Size);
        }
    }
}

// 代码生成入口函数，包括代码块的基础信息
void emitText(Obj *Prog) {
    for (Obj *Fn = Prog; Fn; Fn = Fn->Next) {
        if (!Fn->isFunction) {
            continue;
        }
        printLn("\n  # 定义全局 %s", Fn->Name);
        printLn("  .globl %s", Fn->Name);

        printLn("  # 代码段标签");
        printLn("  .text");
        printLn("# =====%s段开始===============", Fn->Name);
        printLn("# %s段标签", Fn->Name);
        printLn("%s:", Fn->Name);
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
        printLn("  # 将ra寄存器压栈,保存ra的值");
        printLn("  addi sp, sp, -16");
        printLn("  sd ra, 8(sp)");
        // 将fp压入栈中，保存fp的值
        printLn("  # 将fp压栈, fp属于\"被调用者保存\"的寄存器,需要恢复原值");
        printLn("  sd fp, 0(sp)");
        // 将sp写入fp
        printLn("  # 将sp的值写入fp");
        printLn("  mv fp, sp");

        // 偏移量为实际变量所用的栈大小
        printLn("  # sp腾出StackSize大小的栈空间");
        printLn("  addi sp, sp, -%d", Fn->StackSize);

        int I = 0;
        for (Obj *Var = Fn->Params; Var; Var = Var->Next) {
            printLn("  # 将%s寄存器的值存入%s的栈地址", ArgReg[I], Var->Name);
            if (Var->Ty->Size == 1) {
                printLn("  sb %s, %d(fp)", ArgReg[I++], Var->Offset);
            } else {
                printLn("  sd %s, %d(fp)", ArgReg[I++], Var->Offset);
            }
        }

        // 生成语句链表的代码
        printLn("# ===== %s段主体 ============", Fn->Name);
        genStmt(Fn->Body);
        assert(Depth == 0);

        // Epilogue,后语
        // 输出return标签
        printLn("# ===== %s段结束 ============", Fn->Name);
        printLn("# return段标签");
        printLn(".L.return.%s:", Fn->Name);
        // 将fp的值改写回sp
        printLn("  mv sp, fp");
        // 将最早fp保存的值弹栈，恢复fp
        printLn("  # 将最早fp保存的值弹栈,恢复fp和sp");
        printLn("  ld fp, 0(sp)");
        // 将ra寄存器弹栈,恢复ra的值
        printLn("  # 将ra寄存器弹栈,恢复ra的值");
        printLn("  ld ra, 8(sp)");   
        printLn("  addi sp, sp, 16");
    
        // 返回
        printLn("  # 返回a0值给系统调用");
        printLn("  ret");
    }
}

void codegen(Obj *Prog) {
    // 计算局部变量的偏移量
    assignLVarOffsets(Prog);
    // 生成数据
    emitData(Prog);
    // 生成代码
    emitText(Prog);
}
