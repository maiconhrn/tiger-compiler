%{
#include <iostream>
#include "ast/ast.hpp"
#include <string>
#include <llvm/ADT/STLExtras.h>

using namespace AST;

std::unique_ptr<Root> root;

int yylex(void); /* function prototype */

void yyerror(char *s) {
    std::cerr << "syntactic error" << std::endl;
}
%}

%code requires{
#include "ast/ast.hpp"

using namespace AST;
}

%union {
	int pos;
	int ival;
	std::string *sval;
    Root *root;
    Exp *exp;
    Var *var;
    Dec *dec;
    std::vector<std::unique_ptr<Exp>> *exps;
    std::vector<std::unique_ptr<Dec>> *decList;
}

%locations

%token <sval> ID STRING
%token <ival> INT

%type <root> root
%type <exp> exp iff loop let
%type <sval> id
%type <var> lvalue
%type <dec> dec vardec
%type <decList> decs
%type<exps> exps;

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK LBRACE RBRACE DOT
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF BREAK NIL FUNCTION VAR TYPE

%start program

%%

 /* grammar based on ----------> https://www.lrde.epita.fr/~tiger/tiger.html#Syntactic-Specifications */

program: root {root=std::unique_ptr<Root>($1);}
    ;

root: exp;

exp:
    /*Literals.*/
    NIL
    | INT {
        $$ = new IntExp($1);
    }
    | STRING {
        $$ = new StringExp(*$1); delete $1;
    }
    /*Array and record creations.*/
    | id LBRACK exp RBRACK OF exp
    | id LBRACE atribuitions RBRACE
    /*Variables, field, elements of an array.*/
    | lvalue {
        $$ = new VarExp(std::unique_ptr<Var>($1));
    }
    /*Function call.*/
    | id LPAREN arguments RPAREN
    /*Operations.*/
    | MINUS exp
    | exp op exp
    | LPAREN exps RPAREN
    /*Assignment.*/
    | lvalue ASSIGN exp
    /*Control structures.*/
    | iff {
        $$ = $1;
    }
    | loop {
        $$ = $1;
    }
    | BREAK
    | let {
        $$ = $1;
    };

iff:
    IF exp THEN exp {
        $$ = new IfExp(std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4), nullptr);
    }
    | IF exp THEN exp ELSE exp {
        $$ = new IfExp(std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4), std::unique_ptr<Exp>($6));
    };

loop:
    WHILE exp DO exp {
        $$ = new WhileExp(std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4));
    }
    | DO LBRACE exp RBRACE WHILE exp {
        $$ = new DoWhileExp(std::unique_ptr<Exp>($3), std::unique_ptr<Exp>($6));
    }
    | FOR id ASSIGN exp TO exp DO exp {
        $$ = new ForExp(@1.first_line, @1.first_column, *$2, std::unique_ptr<Exp>($4),
                        std::unique_ptr<Exp>($6), 
                        std::unique_ptr<Exp>($8));
        delete $2;
    }

let:
    LET decs IN exps END {
        $$ = new LetExp(std::move(*$2), std::make_unique<SequenceExp>(std::move(*$4)));
    };

lvalue:
    id {
        $$ = new SimpleVar(*$1); delete $1;
    }
    | lvalue DOT id
    | lvalue LBRACK exp RBRACK;

exps: /*empty*/ {
        $$ = new std::vector<std::unique_ptr<Exp>>();
    }
    | exp {
        $$ = new std::vector<std::unique_ptr<Exp>>();
        $$->push_back(std::unique_ptr<Exp>($1));
    }
    | exp SEMICOLON exps {
        $$ = $3;
        $3->push_back(std::unique_ptr<Exp>($1));
    };

decs: /*empty*/ {
        $$ = new std::vector<std::unique_ptr<Dec>>();
    }
    | dec decs {
        $$ = $2;
        $2->push_back(std::unique_ptr<Dec>($1));
    };

dec:
    /*Type declaration.*/
    tydec
    /*Variable declaration.*/
    | vardec {
        $$ = $1;
    }
    /*Function declaration.*/
    | fundec;

tydec:
    TYPE id EQ ty;

ty:
    /*Type alias.*/
    id
    /*Record type definition.*/
    | LBRACE tyfields RBRACE
    /*Array type definition.*/
    | ARRAY OF id;

tyfields: /*empty*/
    | tyfield_list;

tyfield_list:
    id COLON id
    | id COLON id COMMA tyfield_list;

vardec:
    VAR id ASSIGN exp {
        $$ = new VarDec(*$2, "", std::unique_ptr<Exp>($4));
        delete $2;
    }
    | VAR id COLON id ASSIGN exp;

fundec:
    FUNCTION id LPAREN tyfields RPAREN EQ exp
    | FUNCTION id LPAREN tyfields RPAREN COLON id EQ exp

id:
    ID;

op:
    PLUS
    | MINUS
    | TIMES
    | DIVIDE
    | EQ
    | NEQ
    | GT
    | LT
    | GE
    | LE
    | AND
    | OR;

arguments: /*empty*/
    | argument_list;

argument_list:
    exp
    | exp COMMA argument_list;

atribuitions: /*empty*/
    | atribuition_list;

atribuition_list:
    id EQ exp
    | id EQ exp COMMA atribuition_list;