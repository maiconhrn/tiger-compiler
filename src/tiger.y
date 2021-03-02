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
    Type *type;
    Field *field;
    TypeDec *typeDec;
    FunctionDec *functionDec;
    std::vector<std::unique_ptr<Exp>> *exps;
    std::vector<std::unique_ptr<Dec>> *decList;
    std::vector<std::unique_ptr<Field>> *fieldList;
    std::vector<std::unique_ptr<FieldExp>> *fieldExpList;
}

%locations

%token <sval> ID STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK LBRACE RBRACE DOT
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF BREAK NIL FUNCTION VAR TYPE

%type <root> root
%type <exp> exp iff loop let
%type <sval> id
%type <var> lvalue
%type <dec> dec vardec
%type <decList> decs
%type <typeDec> tydec
%type <functionDec> fundec
%type <field> tyfield
%type <fieldList> tyfields tyfield_list
%type <fieldExpList> atribuitions atribuition_list
%type<exps> exps arguments argument_list
%type<type> ty

%left OR
%left AND
%nonassoc EQ NEQ LE LT GT GE
%left PLUS MINUS
%left TIMES DIVIDE
%left UMINUS

%start program

%%

program:
    root {
        root = std::unique_ptr<Root>($1);
    };

root:
    exp {
        $$ = new Root(Location(@1.first_line, @1.first_column), std::unique_ptr<Exp>($1));
    };

exp:
    /*Literals.*/
    NIL {
        $$ = new NilExp(Location(@1.first_line, @1.first_column));
    }
    | INT {
        $$ = new IntExp(Location(@1.first_line, @1.first_column), $1);
    }
    | STRING {
        $$ = new StringExp(Location(@1.first_line, @1.first_column), *$1); 
        delete $1;
    }
    /*Array and record creations.*/
    | id LBRACK exp RBRACK OF exp {
        $$ = new ArrayExp(Location(@1.first_line, @1.first_column),
                *$1,
                std::unique_ptr<Exp>($3),
                std::unique_ptr<Exp>($6));
        delete $1;
    }
    | id LBRACE atribuitions RBRACE {
        $$ = new RecordExp(Location(@1.first_line, @1.first_column),
                *$1,
                std::move(*$3));
        delete $1;
    }
    /*Variables, field, elements of an array.*/
    | lvalue {
        $$ = new VarExp(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Var>($1));
    }
    /*Function call.*/
    | id LPAREN arguments RPAREN {
        $$ = new CallExp(Location(@1.first_line, @1.first_column),
                *$1, std::move(*$3));
        delete $1;
    }
    /*Operations.*/
    | MINUS exp %prec UMINUS {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                    BinaryExp::SUB,
                    std::unique_ptr<Exp>(new IntExp(Location(@1.first_line, @1.first_column), 0)),
                    std::unique_ptr<Exp>($2));
    }
    | exp AND exp {
        $$ = new IfExp(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Exp>($1),
                std::unique_ptr<Exp>($3),
                std::unique_ptr<Exp>(new IntExp(Location(@1.first_line, @1.first_column), 0)));
    }
    | exp OR exp {
        $$ = new IfExp(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Exp>($1),
                std::unique_ptr<Exp>(new IntExp(Location(@1.first_line, @1.first_column), 1)),
                std::unique_ptr<Exp>($3));    
    }
    | exp PLUS exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::ADD, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp MINUS exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::SUB, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp TIMES exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::MUL, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp DIVIDE exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::DIV, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp EQ exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::EQU, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp NEQ exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::NEQU, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp GT exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::GTH, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp LT exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::LTH, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp GE exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::GEQ, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | exp LE exp {
        $$ = new BinaryExp(Location(@1.first_line, @1.first_column),
                BinaryExp::LEQ, 
                std::unique_ptr<Exp>($1), std::unique_ptr<Exp>($3));
    }
    | LPAREN exps RPAREN {
        $$ = new SequenceExp(Location(@1.first_line, @1.first_column),
                std::move(*$2));
    }
    /*Assignment.*/
    | lvalue ASSIGN exp {
        $$ = new AssignExp(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Var>($1),
                std::unique_ptr<Exp>($3));
    }
    /*Control structures.*/
    | iff {
        $$ = $1;
    }
    | loop {
        $$ = $1;
    }
    | let {
        $$ = $1;
    };

iff:
    IF exp THEN exp {
        $$ = new IfExp(Location(@1.first_line, @1.first_column), std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4), nullptr);
    }
    | IF exp THEN exp ELSE exp {
        $$ = new IfExp(Location(@1.first_line, @1.first_column), std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4), std::unique_ptr<Exp>($6));
    };

loop:
    WHILE exp DO exp {
        $$ = new WhileExp(Location(@1.first_line, @1.first_column), std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4));
    }
    | DO exp WHILE exp {
        $$ = new DoWhileExp(Location(@1.first_line, @1.first_column), std::unique_ptr<Exp>($2), std::unique_ptr<Exp>($4));
    }
    | FOR id ASSIGN exp TO exp DO exp {
        $$ = new ForExp(Location(@1.first_line, @1.first_column), *$2, std::unique_ptr<Exp>($4),
                        std::unique_ptr<Exp>($6), 
                        std::unique_ptr<Exp>($8));
        delete $2;
    }
    | BREAK {
        $$ = new BreakExp(Location(@1.first_line, @1.first_column));
    };

let:
    LET decs IN exps END {
        $$ = new LetExp(Location(@1.first_line, @1.first_column), std::move(*$2), std::make_unique<SequenceExp>(Location(@1.first_line, @1.first_column), std::move(*$4)));
    };

lvalue:
    id {
        $$ = new SimpleVar(Location(@1.first_line, @1.first_column), *$1);
        delete $1;
    }
    | id LBRACK exp RBRACK {
        $$ = new SubscriptVar(Location(@1.first_line, @1.first_column),
                std::make_unique<SimpleVar>(Location(@1.first_line, @1.first_column), *$1),
                std::unique_ptr<Exp>($3));
        delete $1;
    }
    | lvalue DOT id {
        $$ = new FieldVar(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Var>($1),
                *$3);
        delete $3;
    }
    | lvalue LBRACK exp RBRACK {
        $$ = new SubscriptVar(Location(@1.first_line, @1.first_column),
                std::unique_ptr<Var>($1),
                std::unique_ptr<Exp>($3));
    };

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
    tydec {
        $$ = $1;
    }
    /*Variable declaration.*/
    | vardec {
        $$ = $1;
    }
    /*Function declaration.*/
    | fundec {
        $$ = $1;
    };

tydec:
    TYPE id EQ ty {
        $$ = new TypeDec(Location(@1.first_line, @1.first_column), *$2, std::unique_ptr<Type>($4));
        delete $2;
    };

ty:
    /*Type alias.*/
    id {
        $$ = new NameType(Location(@1.first_line, @1.first_column), *$1);
        delete $1;
    }
    /*Record type definition.*/
    | LBRACE tyfields RBRACE {
        $$ = new RecordType(Location(@1.first_line, @1.first_column), std::move(*$2));
    }
    /*Array type definition.*/
    | ARRAY OF id {
        $$ = new ArrayType(Location(@1.first_line, @1.first_column), *$3);
        delete $3;
    };

tyfields: /*empty*/ {
        $$=new std::vector<std::unique_ptr<Field>>();
    } 
    | tyfield_list {
        $$ = $1;
    };

tyfield_list:
    tyfield {
        $$ = new std::vector<std::unique_ptr<Field>>();
        $$->push_back(std::unique_ptr<Field>($1));
    }
    | tyfield COMMA tyfield_list {
        $$ = $3;
        $3->push_back(std::unique_ptr<Field>($1));
    };

tyfield:
    id COLON id {
        $$ = new Field(Location(@1.first_line, @1.first_column), *$1, *$3);
        delete $1;
        delete $3;
    };

vardec:
    VAR id ASSIGN exp {
        $$ = new VarDec(Location(@1.first_line, @1.first_column),
                    *$2,
                    "",
                    std::unique_ptr<Exp>($4));
        delete $2;
    }
    | VAR id COLON id ASSIGN exp {
        $$ = new VarDec(Location(@1.first_line, @1.first_column),
                    *$2,
                    *$4,
                    std::unique_ptr<Exp>($6));
        delete $2;
        delete $4;
    };

fundec:
    FUNCTION id LPAREN tyfields RPAREN EQ exp {
        $$ = new FunctionDec(Location(@1.first_line, @1.first_column),
                    *$2,
                    std::make_unique<Prototype>(Location(@1.first_line, @1.first_column), 
                        *$2, std::move(*$4), ""),
                    std::unique_ptr<Exp>($7));
        delete $2;
    }
    | FUNCTION id LPAREN tyfields RPAREN COLON id EQ exp {
        $$ = new FunctionDec(Location(@1.first_line, @1.first_column),
                    *$2,
                    std::make_unique<Prototype>(Location(@1.first_line, @1.first_column),
                        *$2, std::move(*$4), *$7),
                    std::unique_ptr<Exp>($9));
        delete $2;
    }

id:
    ID {
        $$ = $1;
    };

arguments: /*empty*/ {
        $$ = new std::vector<std::unique_ptr<Exp>>();
    }
    | argument_list {
        $$ = $1;
    };

argument_list:
    exp {
        $$ = new std::vector<std::unique_ptr<Exp>>();
        $$->push_back(std::unique_ptr<Exp>($1));
    }
    | exp COMMA argument_list {
        $$ = $3;
        $3->push_back(std::unique_ptr<Exp>($1));
    };

atribuitions: /*empty*/ {
        $$ = new std::vector<std::unique_ptr<FieldExp>>();
    }
    | atribuition_list {
        $$ = $1;
    };

atribuition_list:
    id EQ exp {
        $$ = new std::vector<std::unique_ptr<FieldExp>>();
        $$->push_back(std::make_unique<FieldExp>(Location(@1.first_line, @1.first_column),
                        *$1,
                        std::unique_ptr<Exp>($3)));
        delete $1;
    }
    | id EQ exp COMMA atribuition_list {
        $$ = $5;
        $5->push_back(std::make_unique<FieldExp>(Location(@1.first_line, @1.first_column),
                        *$1, std::unique_ptr<Exp>($3)));
        delete $1;
    };