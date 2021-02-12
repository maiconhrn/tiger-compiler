#include <iostream>
#include <string>
#include "y.tab.hpp"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

extern YYSTYPE yylval;

int yylex(void); /* prototype for the lexing function */

extern FILE *yyin;

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

    for (;;) {
        tok = yylex();
        if (tok == 0) break;
        switch (tok) {
            case ID:
            case STRING:
                cout << tok << " - " << *(yylval.sval) << endl;
                break;
            case INT:
                cout << tok << " - " << yylval.ival << endl;
                break;
            default:
                cout << tok << endl;
        }
    }

    exit(0);
}
