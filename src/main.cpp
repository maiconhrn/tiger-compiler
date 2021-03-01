#include <iostream>
#include <string>
#include "ast/ast.hpp"
#include "tiger.parser.hpp"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

extern YYSTYPE yylval;
extern std::unique_ptr<AST::Root> root;

//int yylex(void); /* prototype for the lexing function */

extern int yyparse(void); /* prototype for the syntaxing function */

extern FILE *yyin;

void parse(std::string fname) {
    if (yyparse() == 0) /* parsing worked */
        fprintf(stderr, "Parsing successful!\n");
    else
        fprintf(stderr, "Parsing failed\n");
}

std::string toknames[] = {
        "ID",
        "STRING",
        "INT",
        "COMMA",
        "COLON",
        "SEMICOLON",
        "LPAREN",
        "RPAREN",
        "LBRACK",
        "RBRACK",
        "LBRACE",
        "RBRACE",
        "DOT",
        "PLUS",
        "MINUS",
        "TIMES",
        "DIVIDE",
        "EQ",
        "NEQ",
        "LT",
        "LE",
        "GT",
        "GE",
        "AND",
        "OR",
        "ASSIGN",
        "ARRAY",
        "IF",
        "THEN",
        "ELSE",
        "WHILE",
        "FOR",
        "TO",
        "DO",
        "LET",
        "IN",
        "END",
        "OF",
        "BREAK",
        "NIL",
        "FUNCTION",
        "VAR",
        "TYPE"
};

std::string tokname(int tok) {
    return tok < 258 || tok > 299 ? "BAD_TOKEN" : toknames[tok - 258];
}

int main(int argc, char **argv) {
    std::string fname;
    int tok;
    if (argc != 2) {
        cerr << "Usage: <executable> filename" << endl;
        exit(1);
    }
    fname = argv[1];
    yyin = fopen(fname.c_str(), "r");
    if (!yyin) {
        cerr << "Cannot open file: " << fname << endl;
        exit(1);
    }

    // for (;;) {
    //     tok = yylex();
    //     if (tok == 0) break;
    //     switch (tok) {
    //         case ID:
    //         case STRING:
    //             cout << tokname(tok) << " - " << *(yylval.sval) << endl;
    //             break;
    //         case INT:
    //             cout << tokname(tok) << " - " << yylval.ival << endl;
    //             break;
    //         default:
    //             cout << tokname(tok) << endl;
    //     }
    // }

    parse(fname);

    if (root) {
        CodeGenContext codeGenContext;
        std::vector<VarDec *> v;
        root->traverse(v, codeGenContext);
    }

    exit(0);
}
