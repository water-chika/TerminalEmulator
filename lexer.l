%{
#include "parser.tab.h"
%}

ID       [a-z][a-z0-9]*

%x	STATEMENTS
%%
"<-"	{yylval = strdup("<-"); return LEFT_ARROW;}
"{"	{BEGIN(STATEMENTS);}
<STATEMENTS>"}"	{BEGIN(INITIAL);}
<STATEMENTS>[^}]*	{yylval = strdup(yytext);return INIT_STATEMENTS;}
{ID}	{yylval = strdup(yytext); return ID;}
"\n"	{yylval = strdup(yytext); return END_OF_LINE;}
%%
