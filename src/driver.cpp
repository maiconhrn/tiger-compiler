#include <iostream>
#include <string>
#include "y.tab.hpp"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

extern YYSTYPE yylval;

//int yylex(void); /* prototype for the lexing function */

extern int yyparse(void); /* prototype for the syntaxing function */

extern FILE *yyin;

void parse(std::string fname) {
    if (yyparse() == 0) /* parsing worked */
        fprintf(stderr, "Parsing successful!\n");
    else
        fprintf(stderr, "Parsing failed\n");
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

    parse(fname);

    exit(0);
}
