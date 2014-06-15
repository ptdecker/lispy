/* Provide a basic REPL (read, evaluate, print loop) example
 *
 *
 * LICENSE INFO
 * ------------
 *
 * The licensed libraries used by this project are:
 *    - Brian Fox's GNU Readline under GPL v3 (now maintained by Chet Ramey)
 *           c.f. http://cnswww.cns.cwru.edu/php/chet/readline/rltop.html
 *    - Daniel Holden's MPC under BSD3
 *           c.f. https://github.com/orangeduck/mpc
 *
 * Due to the inclusion of GNU Readline, this project is also licensed under GPL v3
 */

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

/* Evaluate unary operators */

long eval_unary_op(long x, char* op) {

	if (strcmp(op, "-") == 0) {
		return (- x);
	}

	return 0;

}

/* Evaluate n-ary operators */

long eval_nary_op(long x, char* op, long y) {

	if ((strcmp(op, "+") == 0) || (strcmp(op, "add") == 0)) {
		return x + y;
	}

	if ((strcmp(op, "-") == 0) || (strcmp(op, "sub") == 0)) {
		return x - y;
	}

	if ((strcmp(op, "*") == 0) || (strcmp(op, "mul") == 0)) {
		return x * y;
	}

	if ((strcmp(op, "/") == 0) || (strcmp(op, "div") == 0)) {
		return x / y;
	}

	if ((strcmp(op, "%") == 0) || (strcmp(op, "mod") == 0)) {
		return x % y;
	}

	if ((strcmp(op, "^") == 0) || (strcmp(op, "exp") == 0)) {
		return pow(x, y);
	}

	if (strcmp(op, "min") == 0) {
		return (x < y) ? x : y;
	}

	if (strcmp(op, "max") == 0) {
		return (x > y) ? x : y;
	}

	return 0;

}

/* Recursive evalution function */

long eval(mpc_ast_t* t) {

	/* If tagged as a number return it directly; otherwise, handle expression */

	if (strstr(t->tag, "number")) {
		return atoi(t->contents);
	}

	/* The operator is always the second child */

	char* op = t->children[1]->contents;

	/* We store the third child in 'x' */

	long x = eval(t->children[2]);

	/* Iterate over the remaining children combining them using our operator */

	if(strstr(t->children[3]->tag, "expr")) {

		/* Handle binary and n-ary operatarors */

		int i = 3;
		while (strstr(t->children[i]->tag, "expr")) {
			x = eval_nary_op(x, op, eval(t->children[i]));
			i++;
		}

	} else {

		/* Handle unary operators */

		x = eval_unary_op(x, op);

	}

	return x;

}

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
    		operator : '+' | '-' | '*' | '/' | '%' | '^' | \
    		           \"add\" | \"sub\" | \"mul\" | \"div\" | \"mod\" | \"exp\" | \
    		           \"min\" | \"max\" ; \
    		expr     : <number> | '(' <operator> <expr>+ ')' ; \
    		lispy    : /^/ <operator> <expr>+ /$/ ; \
  		",
  		Number, Operator, Expr, Lispy);

	/* Display Initialization Header */

	puts("Lispy Couch Version 0.0.3");
	puts("Press 'ctrl-c' to exit\n");

	/* Enter into REPL */

	while (true) {

		/* Read and then parse the input */

		char* input = readline("lc> ");
		add_history(input);

		mpc_result_t r;

		/* Evaluate */

		if (mpc_parse("<stdin>", input, Lispy, &r)) {

			long result = eval(r.output);

			/* Print */

			printf("%li\n", result);
			mpc_ast_delete(r.output);

		} else {

			mpc_err_print(r.error);
			mpc_err_delete(r.error);

		}

		/* Loop */

		free(input);
	}

	/* Clean up and go home now that the hard work is done */

	mpc_cleanup(4, Number, Operator, Expr, Lispy);

	return EXIT_SUCCESS;
}

