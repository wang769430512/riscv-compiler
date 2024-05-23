
#include "rvcc.h"

// {TY_INT}构造了一个数据结构，(Type)强制类型转换为struct
Type *TyChar = &(Type){TY_CHAR, 1};
// 全局变量TyInt,用来将Type赋值为int类型
Type *TyInt = &(Type){TY_INT, 8};

// 判断type是否为整数
bool isInteger(Type *Ty) {
    return Ty->Kind == TY_CHAR || Ty->Kind == TY_INT;
}

// 复制类型
Type *copyType(Type *Ty) {
    Type *Ret = calloc(1, sizeof(Type));
    *Ret = *Ty;
    return Ret;
}

// 指针类型，并且指向基类
Type *pointerTo(Type *Base) {
    Type *Ty = calloc(1, sizeof(Type));
    Ty->Kind = TY_PTR;
    Ty->Size = 8;
    Ty->Base = Base;
    return Ty;
}

// 函数类型，并赋返回类型
Type *funcType(Type *ReturnTy) {
    Type *Ty = calloc(1, sizeof(Type));
    Ty->Kind = TY_FUNC;
    Ty->ReturnTy = ReturnTy;
    return Ty;
}

// 构造数组类型，传入数组基类、个数
Type *arrayOf(Type *Base, int Len) {
    Type *Ty = calloc(1, sizeof(Type));
    Ty->Kind = TY_ARRAY;
    // 数组大小为所有元素大小之和
    Ty->Size = Base->Size * Len;
    Ty->Base = Base;
    Ty->ArrayLen = Len;

    return Ty;
}

// 为节点增加类型
void addType(Node *Nd) {
    if (!Nd || Nd->Ty) {
        return;
    }

    // 递归访问所有节点以增加类型
    addType(Nd->LHS);
    addType(Nd->RHS);
    addType(Nd->Cond);
    addType(Nd->Then);
    addType(Nd->Els);
    addType(Nd->Init);
    addType(Nd->Inc);

    // 访问链表内的所有节点以增加类型
    for (Node *N=Nd->Body; N; N=N->Next) {
        addType(N);
    }

    // 访问链表内的所有参数节点以增加类型
    for (Node *N=Nd->Args; N; N=N->Next) {
        addType(N);
    }

    switch(Nd->Kind) {
    // 将节点类型设别 节点左部的类型
    case ND_ADD:
    case ND_SUB:
    case ND_MUL:
    case ND_DIV:
    case ND_NEG:
        Nd->Ty = Nd->LHS->Ty;
        return;
        // 将节点类型设置为节点左部的类型
        // 左部不能是数组节点
    case ND_ASSIGN:
        if (Nd->LHS->Ty->Kind == TY_ARRAY)
            errorTok(Nd->LHS->Tok, "not an lvalue");
        Nd->Ty = Nd->LHS->Ty;
        return;
    // 将节点类型设为 int
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_NUM:
    case ND_FUNCALL:
        Nd->Ty = TyInt;
        return;
    // 将节点类型设为 变量的类型
    case ND_VAR:
        Nd->Ty = Nd->Var->Ty;
        return;
    case ND_COMMA:
        Nd->Ty = Nd->RHS->Ty;
        return;
    // 将节点类型设为 指针，并指向左部的类型
    case ND_ADDR:
        // Type *Ty = Nd->LHS->Ty;
        // 左部如果是数组，则为指向数组基类的指针
        if (Nd->LHS->Ty->Kind == TY_ARRAY) 
            Nd->Ty = pointerTo(Nd->LHS->Ty->Base);
        else
            Nd->Ty = pointerTo(Nd->LHS->Ty);
        return;
    case ND_DEREF:
        // 如果不存在基类，则无法解引用
        if (!Nd->LHS->Ty->Base) {
            errorTok(Nd->Tok, "invalid pointer dereference");
        }
        Nd->Ty = Nd->LHS->Ty->Base;
        return;
    // 节点类型为 最后的表达式语句的类型
    case ND_STMT_EXPR:
        if (Nd->Body) {
            Node *Stmt = Nd->Body;
            while (Stmt->Next) {
                Stmt = Stmt->Next;
            }
            if (Stmt->Kind == ND_EXPR_STMT) {
                Nd->Ty = Stmt->LHS->Ty;
                return;
            }
        }
        errorTok(Nd->Tok, "statment expresion returning void is not supported");
        return;
    default:
        break;
    }
}
