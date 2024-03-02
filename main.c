#include "rvcc.h" 

int main(int argc, char **Argv)
{
    
    if (argc != 2)
    {
        error("%s: invalid number of arguments", Argv[0]);
    }
    
    // 解释Argv[1],生成终结符流
    Token *Tok = tokenizeFile(Argv[1]);

    // 解释终结符流  
    Obj *prog = parse(Tok);
    
    codegen(prog);
    
    return 0;
}
