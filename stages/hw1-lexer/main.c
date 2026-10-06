#include <stdio.h>
#include "token_types.h"

extern int num_lines;
extern char *yytext;
extern int yylex (void);

int main()
{
  int ret;
  while ((ret = yylex()) != 0) {
    printf("Found an ERROR %s\n", yytext);
  }
  printf("Total number of lines: %d\n", num_lines);
  return 0;
}

int yywrap ()
{
  return 1;
}
