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
    | decs;

exp:
    /*Literals.*/
    NIL
    | INT
    | STRING
    /*Array and record creations.*/
    | type_id LBRACK exp RBRACK OF exp
    | type_id LBRACE atribuitions RBRACE
    /*Variables, field, elements of an array.*/
    | lvalue
    /*Function call.*/
    | ID LPAREN arguments RPAREN
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
    | FOR ID ASSIGN exp TO exp DO exp
    | BREAK
    | LET decs IN exps END;

lvalue: ID
    | lvalue DOT ID
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
    TYPE ID EQ ty;

ty:
    /*Type alias.*/
    type_id
    /*Record type definition.*/
    | LBRACE tyfields RBRACE
    /*Array type definition.*/
    | ARRAY OF type_id;

tyfields: /*empty*/
    | tyfield_list;

tyfield_list:
    ID COLON type_id
    | ID COLON type_id COMMA tyfield_list;

vardec:
    VAR ID ASSIGN exp
    | VAR ID COLON type_id ASSIGN exp;

fundec:
    FUNCTION ID LPAREN tyfields RPAREN ASSIGN exp
    | FUNCTION ID LPAREN tyfields RPAREN COLON type_id ASSIGN exp

type_id:
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
    ID EQ exp
    | ID EQ exp COMMA atribuition_list;