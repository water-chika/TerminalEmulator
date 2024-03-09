#include "behavior.hpp"
#include <FlexLexer.h>

int main() {
	yyFlexLexer lexer;
	while (lexer.yylex());
	return 0;
}
