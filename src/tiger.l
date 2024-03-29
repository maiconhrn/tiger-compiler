%{
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include "ast/ast.hpp"
#include "tiger.parser.hpp"

#define BUFSIZE 65535

int charPos = 1;
std::stringstream strbuf;
char *strptr = NULL;
int commentDepth = 0;
int lexline = 1;
int lexcol = 0;

extern YYLTYPE yylloc;

void adjust() {
        yylloc.first_line = lexline;
        yylloc.first_column = lexcol + 1;

        for(int i = 0; yytext[i] != '\0'; i++) {
                if (yytext[i] == '\n') {
                        lexline++;
                        lexcol = 0;
                } else if (yytext[i] == '\t') {
                        lexcol += 8 - (lexcol % 8);
                } else {
                        lexcol++;
                }
        }

        yylloc.last_line = lexline;
        yylloc.last_column = lexcol;

        charPos+=yyleng;
}

extern "C" int yywrap(void) {
        charPos=1;
        return 1;
}

%}
%x COMMENT STR
%%
[ \t]	                {adjust(); continue;}
(\n|\r\n)               {adjust(); continue;}
"*"                     {adjust(); return TIMES;}
"/"                     {adjust(); return DIVIDE;}
"/*"                    {adjust(); BEGIN(COMMENT); commentDepth++;}
<COMMENT>{
	"/*"            {adjust(); commentDepth++;}
	"*/"            {adjust(); if (--commentDepth == 0) BEGIN(INITIAL);}
	[^\n]           {adjust();}
        (\n|\r\n)	{adjust();}
        <<EOF>>         {adjust(); std::cerr << "lexal error: illegal comment (has not closed the block): line: " << lexline << std::endl; BEGIN(INITIAL);}
        
}
"array"                 {adjust(); return ARRAY;}
"break"                 {adjust(); return BREAK;}
"do"	                {adjust(); return DO;}
"end"                   {adjust(); return END;}
"else"                  {adjust(); return ELSE;}
"for"  	                {adjust(); return FOR;}
"function"              {adjust(); return FUNCTION;}
"if"	                {adjust(); return IF;}
"in"                    {adjust(); return IN;}
"let"	                {adjust(); return LET;}
"nil"	                {adjust(); return NIL;}
"of"	                {adjust(); return OF;}
"then"                  {adjust(); return THEN;}
"to"	                {adjust(); return TO;}
"type"                  {adjust(); return TYPE;}
"while"                 {adjust(); return WHILE;}
"var"                   {adjust(); return VAR;}
[a-zA-Z][a-zA-Z0-9_]*   {adjust(); yylval.sval = new std::string(yytext); return ID;}
[0-9]+	                {adjust(); yylval.ival = atoi(yytext); return INT;}
"+"                     {adjust(); return PLUS;}
"-"                     {adjust(); return MINUS;}
"&"	                {adjust(); return AND;}
"|"	                {adjust(); return OR;}
","	                {adjust(); return COMMA;}
"."                     {adjust(); return DOT;}
":"	                {adjust(); return COLON;}
";"	                {adjust(); return SEMICOLON;}
"("	                {adjust(); return LPAREN;}
")"                     {adjust(); return RPAREN;}
"["                     {adjust(); return LBRACK;}
"]"                     {adjust(); return RBRACK;}
"{"                     {adjust(); return LBRACE;}
"}"                     {adjust(); return RBRACE;}
"="                     {adjust(); return EQ;}
"<>"                    {adjust(); return NEQ;}
"<"                     {adjust(); return LT;}
"<="                    {adjust(); return LE;}
">"                     {adjust(); return GT;}
">="                    {adjust(); return GE;}
":="                    {adjust(); return ASSIGN;}

\" {adjust(); BEGIN(STR); }
<STR>{
\" 			 {adjust(); yylval.sval = new std::string(strbuf.str()); strbuf.clear(); strbuf.str(std::string()); BEGIN(INITIAL); return STRING;}
        \\n	         {adjust(); strbuf << "\n";}
        \\b	         {adjust(); strbuf << "\b";}
        \\t	         {adjust(); strbuf << "\t";}
        \\\^[GHIJLM]	 {adjust(); strbuf << yytext;}
        \\[0-9]{3}	 {adjust(); strbuf << yytext;}
        \\\"    	 {adjust(); strbuf << yytext;}
	\\[ \n\t\r\f]+\\ {adjust();}
        \\(.|\n)	 {adjust(); std::cerr << "lexal error: illegal token: line: " <<  lexline << std::endl;}
        (\n|\r\n)	 {adjust(); std::cerr <<  "lexal error: illegal token: line: " <<  lexline << std::endl;}
        [^\"\\\n(\r\n)]+ {adjust(); strbuf << yytext;}
}
.	 {adjust(); std::cerr << "lexal error: illegal token: line: " <<  lexline << std::endl;}
%%
