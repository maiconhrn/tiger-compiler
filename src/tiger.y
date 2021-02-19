%{
#include <iostream>
#include <string>

int yylex(void); /* function prototype */

void yyerror(char *s)
{
    std::cerr << "ERR: bla" << std::endl;
}
%}

%union {
	int pos;
	int ival;
	std::string *sval;
}

%locations

%token <sval> ID STRING
%token <ival> INT

%token
  COMMA COLON SEMICOLON LPAREN RPAREN LBRACK RBRACK LBRACE RBRACE DOT
  PLUS MINUS TIMES DIVIDE EQ NEQ LT LE GT GE AND OR ASSIGN
  ARRAY IF THEN ELSE WHILE FOR TO DO LET IN END OF BREAK NIL FUNCTION VAR TYPE

%start program

%%

 /* grammar based on ----------> https://www.lrde.epita.fr/~tiger/tiger.html#Syntactic-Specifications */

program:
    exp
    /*| decs*/; /* TODO: retirar pois em tiger um programa e uma expressao nao uma declaracao */

exp:
    /*Literals.*/
    NIL
    | INT
    | STRING
    /*Array and record creations.*/
    | id LBRACK exp RBRACK OF exp
    | id LBRACE atribuitions RBRACE
    /*Variables, field, elements of an array.*/
    | lvalue
    /*Function call.*/
    | id LPAREN arguments RPAREN
    /*Operations.*/
    | MINUS exp
    | exp op exp
    | LPAREN exps RPAREN
    /*Assignment.*/
    | lvalue ASSIGN exp
    /*Control structures.*/
    | IF exp THEN exp
    | IF exp THEN exp ELSE exp
    | WHILE exp DO exp
    | DO LBRACE exp RBRACE WHILE exp
    | FOR id ASSIGN exp TO exp DO exp
    | BREAK
    | LET decs IN exps END;

lvalue: id
    | lvalue DOT id
    | lvalue LBRACK exp RBRACK;

exps: /*empty*/
    | exp_list;

exp_list:
    exp
    | exp SEMICOLON exp_list;

decs: /*empty*/
    | dec_list;

dec_list:
    dec
    | dec dec_list;

dec:
    /*Type declaration.*/
    tydec
    /*Variable declaration.*/
    | vardec
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
    VAR id ASSIGN exp
    | VAR id COLON id ASSIGN exp;

fundec:
    FUNCTION id LPAREN tyfields RPAREN ASSIGN exp
    | FUNCTION id LPAREN tyfields RPAREN COLON id ASSIGN exp

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