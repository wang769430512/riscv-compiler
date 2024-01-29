#include "rvcc.h" 

int main(int argc, char **argv)
{
    
    if (argc != 2)
    {
        error("%s: invalid number of arguments", argv[0]);
    }
    
    // 解释Argv[1],生成终结符流
    Token *Tok = tokenize(argv[1]);

    // 解释终结符流  
    Obj *prog = parse(Tok);
    
    codegen(prog);
    
    return 0;
}
