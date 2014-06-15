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

/* Declare lval structure to carry errors and support multiple types */

union {
	long num;
	int  err;
} lval_data;

typedef struct {
	int type;
	union {
		long num;
		int  err;
	} data;
} lval;

/* Create enumerated types for supported lval types and error codes */

enum lval_types {
	LVAL_NUM,
	LVAL_ERR
};

enum err_codes {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM
};

/* Create a new number type lval */

lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.data.num  = x;
	return v;
}

/* Create a new error type lval */

lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.data.err = x;
	return v;
}

/* Print an lval error code */

void lval_print_err(lval v) {
	if (v.type == LVAL_ERR) {
		switch (v.data.err) {
			case LERR_BAD_NUM:
				printf("Error: Encountered an invalid number!");
				break;
			case LERR_BAD_OP:
				printf("Error: Encountered an invalid operator!");
				break;
			case LERR_DIV_ZERO:
				printf("Error: Attempted to divide by zero!");
				break;
			default:
				printf("Error: An unknown error of error code '%d' occurred!", v.data.err);
		}
	} else {
		printf("Error: Asked to print an error message but not passed an error type of '%d'!", v.type);
	}
}


/* Print an lval */

void lval_print(lval v) {
	switch (v.type) {
		case LVAL_NUM:
			printf("%li", v.data.num);
			break;
		case LVAL_ERR:
			lval_print_err(v);
			break;
		default:
			printf("Error: Attempted to print an unknown type of type code '%d'!", v.type);
	}
}

/* Print an lval followed by a new line */

void lval_println(lval v) {
	lval_print(v);
	putchar('\n');
}

/* Evaluate unary operators */

lval eval_unary_op(lval x, char* op) {

	/* Handle being passed a value that is already an error */

	if (x.type == LVAL_ERR) {
		return x;
	}

	/* Evaluate the unary operation properly handling any errors encountered */

	if (strcmp(op, "-") == 0) {
		return lval_num(- x.data.num);
	}

	/* If we made it this far, then we don't yet know how to handle the operator */

	return lval_err(LERR_BAD_OP);

}

/* Evaluate n-ary operators */

lval eval_nary_op(lval x, char* op, lval y) {

	/* Handle being passed a value that is already an error */

	if (x.type == LVAL_ERR) {
		return x;
	}

	if (y.type == LVAL_ERR) {
		return y;
	}

	/* Evaluate the first binary values of what could be a n-ary operation and
	 * properly handle any errors encountered.
	 */

	if ((strcmp(op, "+") == 0) || (strcmp(op, "add") == 0)) {
		return lval_num(x.data.num + y.data.num);
	}

	if ((strcmp(op, "-") == 0) || (strcmp(op, "sub") == 0)) {
		return lval_num(x.data.num - y.data.num);
	}

	if ((strcmp(op, "*") == 0) || (strcmp(op, "mul") == 0)) {
		return lval_num(x.data.num * y.data.num);
	}

	if ((strcmp(op, "/") == 0) || (strcmp(op, "div") == 0)) {
		if (y.data.num == 0) {
			return lval_err(LERR_DIV_ZERO);
		} else {
			return lval_num(x.data.num / y.data.num);
		}
	}

	if ((strcmp(op, "%") == 0) || (strcmp(op, "mod") == 0)) {
		return lval_num(x.data.num % y.data.num);
	}

	if ((strcmp(op, "^") == 0) || (strcmp(op, "exp") == 0)) {
		return lval_num(pow(x.data.num, y.data.num));
	}

	if (strcmp(op, "min") == 0) {
		return lval_num((x.data.num < y.data.num) ? x.data.num : y.data.num);
	}

	if (strcmp(op, "max") == 0) {
		return lval_num((x.data.num > y.data.num) ? x.data.num : y.data.num);
	}

	/* If we made it this far, then we don't yet know how to handle the operator */

	return lval_err(LERR_BAD_OP);

}

/* Recursive evalution function */

lval eval(mpc_ast_t* t) {

	/* If tagged as a number return it directly; otherwise, handle expression */

	if (strstr(t->tag, "number")) {
		errno = 0;		
		long x = strtol(t->contents, NULL, 10);
		if (errno != ERANGE) {
			return lval_num(x);
		} else {
			return lval_err(LERR_BAD_NUM);
		}
	}

	/* The operator is always the second child */

	char* op = t->children[1]->contents;

	/* We store the third child in 'x' */

	lval x = eval(t->children[2]);

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
    		           \"min\" | \"max\" | \"and\" ; \
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

			lval result = eval(r.output);

			/* Print */

			lval_println(result);
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

