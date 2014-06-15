/* Provide a basic REPL (read, evaluate, print loop) example */

/* Requires sudo apt-get install libedit-dev */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* Include Daniel Holden's MPC "...lightweight and powerful Parser Combinator" library
 *
 * c.f. https://github.com/orangeduck/mpc
 * c.f. https://www.theorangeduck.com
 */

#include "mpc.h"

/* Accomidate compiling on Windows which doesn't have editline available */

#ifdef _WIN32

#include <string.h>

static char buffer[2048];

/* Fake readline function */

char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer)+1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy)-1] = '\0';
  return cpy;
}

/* Fake add_history function */

void add_history(char* unused) {}

/* Otherwise include the editline headers */

#else

#include <editline/readline.h>
#include <editline/history.h>

#endif

/* Start REPL */

int main (int argc, char** argv) {

	/* Initialize the mpc parser for Polish Notation */

	mpc_parser_t* Number   = mpc_new("number");
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr     = mpc_new("expr");
	mpc_parser_t* Lispy    = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
  		" \
    		number   : /-?[0-9]+/ ; \
    		operator : '+' | '-' | '*' | '/' | '%' | \
    		           \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" ; \
    		expr     : <number> | '(' <operator> <expr>+ ')' ; \
    		lispy    : /^/ <operator> <expr>+ /$/ ; \
  		",
  		Number, Operator, Expr, Lispy);

	/* Display Initialization Header */

	puts("Lispy Couch Version 0.0.2");
	puts("Press 'ctrl-c' to exit\n");

	/* Enter into REPL */

	while (true) {

		/* Read */

		char* input = readline("lc> ");
		add_history(input);

		/* Evaluate */

		mpc_result_t r;

		if (mpc_parse("<stdin>", input, Lispy, &r)) {
			mpc_ast_print(r.output);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		/* Print */

		printf("%s\n", input);

		/* Loop */

		free(input);
	}

	/* Clean up and go home now that the hard work is done */

	mpc_cleanup(4, Number, Operator, Expr, Lispy);

	return EXIT_SUCCESS;
}

