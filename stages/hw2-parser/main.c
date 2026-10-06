#include <stdio.h>
#include "types.h"
#include "y.tab.h"

extern int yyparse();

int yywrap ()
{
  return 1;
}

int yyerror(char *s)
{
  fprintf(stderr, "%s\n", s);
  return 0;
}

int main()
{
  yyparse();
  return 0;
}
