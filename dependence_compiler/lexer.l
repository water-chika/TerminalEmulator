%{
#include "parser.tab.h"
%}

ID       [a-zA-Z_][a-zA-Z0-9_]*

%x	STATEMENTS
	int brace_depth = 0;
%%
"<-"	{yylval = "<-"; return LEFT_ARROW;}
"{"	{ ++brace_depth; BEGIN(STATEMENTS);}
<STATEMENTS>"{"	{ ++brace_depth; return LEFT_BRACE; }
<STATEMENTS>"}"	{ --brace_depth; if (brace_depth==0) {BEGIN(INITIAL);} else {return RIGHT_BRACE;}}
<STATEMENTS>[^{}]*	{yylval = yytext;return INIT_STATEMENTS;}
{ID}	{yylval = yytext; return ID;}
"\n"	{yylval = yytext; return END_OF_LINE;}
%%
