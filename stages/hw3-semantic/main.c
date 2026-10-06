#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "types.h"
#include "y.tab.h"

int debug_parsing = 0;
int debug_ast = 0;
int debug_semantic = 0;

int yywrap ()
{
  return 1;
}

int yyerror(char *s)
{
  error("Error: %s at line %d.\n", s, lineno);
}

void usage(char *exec)
{
  error("Usage: %s [-a] [-p] [-h] < file.c\n"
        "Optional arguments:\n"
        "  -h        Show help options\n"
        "  -a        Print AST\n"
        "  -p        Print parsing information\n"
        "\n", exec);
}

int main(int argc, char **argv)
{
  int opt;
  while ((opt = getopt(argc, argv, "apsh")) != -1) {
    switch (opt) {
      case 'a':
        debug_ast = 1;
        break;
      case 'p':
        debug_parsing = 1;
        break;
      case 's':
        debug_semantic = 1;
        break;
      case 'h':
      default: /* '?' */
        usage(argv[0]);
    }
  }

  yyparse();
  print_ast();
  semantic_analysis();

  fprintf(stdout, "Compilation done.\n");
  return 0;
}
