%{
#include <stdio.h>
#include "parser.h"
#include "lexer.h"
%}

%header
%token ID LEFT_ARROW INIT_STATEMENTS END_OF_LINE
%define api.value.type {const char*}

%%

all:
  depend all
| init all
| %empty
;

depend: ID LEFT_ARROW ID END_OF_LINE { add_depend($1, $3);printf("depend: %s, %s\n", $1, $3); }
;

init: ID INIT_STATEMENTS END_OF_LINE { add_init($1, $2); printf("init: %s, %s\n", $1, $2);}
;

%%

void yyerror(const char* err) {
    fprintf(stderr, "err: %s\n", err);
}
