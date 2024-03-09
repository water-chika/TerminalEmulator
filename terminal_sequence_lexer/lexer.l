%{

#include "behavior.hpp"

%}

%option noyywrap c++

ESC	\x1b
NUM	[0-9]*

%%

${ESC}M	;
${ESC}7	;
${ESC}8	;
${ESC}"["${NUM}m ;

%%
