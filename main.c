#include "rvcc.h" 

int main(int argc, char **argv)
{
    
    if (argc != 2)
    {
        error("%s: invalid number of arguments", argv[0]);
    }
    

    Token *Tok = tokenize(argv[1]);

    Node *Node = parse(Tok);
    
    codegen(Node);

    return 0;
}
