#include <iostream>
#include <string>
#include <sstream>
#include "ast/ast.hpp"
#include "tiger.parser.hpp"

using std::cerr;
using std::cin;
using std::cout;
using std::endl;

extern YYSTYPE yylval;
extern std::unique_ptr<AST::Root> root;

extern int yyparse(void); /* prototype for the syntaxing function */

extern FILE *yyin;

void syntacticAnalisys() {
    if (yyparse() == 0) {
        cout << "Syntactic analysis successful!" << endl;
    } else {
        cerr << "Syntactic analysis failed" << endl;
        exit(EXIT_FAILURE);
    }
}

void semanticAnalisys(CodeGenContext &context) {
    if (root) {
        if (root->traverse(context)) {
            cout << "Semantic analysis successful!" << endl;
        } else {
            cerr << "Semantic analysis failed" << endl;
            exit(EXIT_FAILURE);
        }
    }
}

void codegen(CodeGenContext &context) {
    if (root) {
        root->codegen(context);

        if (!context.hasError) {
            cout << "Codegen successful!" << endl;
        } else {
            cerr << "Codegen failed" << endl;
            exit(EXIT_FAILURE);
        }
    }
}

void printABS() {
    if (root) {
        root->print(0);
    }
}

void executablegen(CodeGenContext &context) {
    std::stringstream ss;

    ss << "clang++ " << context.outputFileO << " ";
    for (const auto &lib : context.libs) {
        ss << lib << " ";
    }
    ss << "-o " << context.outputFileE;

    if (system(ss.str().c_str()) == 0) {
        cout << "Executable: \"" << context.outputFileE << "\" generated!" << endl;
    } else {
        cerr << "Executable: \"" << context.outputFileE << "\" not generated!" << endl;
        exit(EXIT_FAILURE);
    }

}

int main(int argc, char **argv) {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    std::string fname;
    CodeGenContext codeGenContext;

    std::vector<std::string> args(argv, argv + argc);

    if (args.size() < 3 ||
        std::find(args.begin(), args.end(), "-p") == args.end()) {
        cerr << "Usage: <executable> -p filename" << endl
             << "opts: \"-a\" : print generated ABS for \"-p\" file" << endl
             << "      \"-i {{output file}}\" : output LLVM IR text representation for \"-p\" file" << endl
             << "      \"-o {{output file}}\" : output the compiled executable for \"-p\" file" << endl
             << "      \"-l{{lib path}}\" : add lib to be compiled with \"-p\" file" << endl;
        exit(EXIT_FAILURE);
    }

    fname = *(std::find(args.begin(), args.end(), "-p") + 1);
    yyin = fopen(fname.c_str(), "r");
    if (!yyin) {
        cerr << "Cannot open file: " << fname << endl;
        exit(EXIT_FAILURE);
    }

    auto _i = std::find(args.begin(), args.end(), "-i");
    if (_i != args.end()) {
        codeGenContext.outputFileI = *(++_i);
    }

    auto _o = std::find(args.begin(), args.end(), "-o");
    if (_o != args.end()) {
        codeGenContext.outputFileE = *(++_o);
    }

    auto _l = args.begin();
    while ((_l = std::find_if(_l,
                              args.end(),
                              [](const std::string &str) {
                                  return str.find("-l") != std::string::npos;
                              })) != args.end()) {
        auto l = *(_l++);
        codeGenContext.libs.push_back(l.substr(2, l.size()));
    }

    syntacticAnalisys();
    semanticAnalisys(codeGenContext);

    if (std::find(args.begin(), args.end(), "-a") != args.end()) {
        printABS();
    }

    codegen(codeGenContext);
    executablegen(codeGenContext);

    exit(EXIT_SUCCESS);
}
