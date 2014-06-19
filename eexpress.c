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

/* Define macros */

#define LASSERT(args, cond, err) if (!(cond)) { lval_del(args); return lval_err(err); }

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

typedef struct lval {
	int    type;        // lisp value type
	long   num;         // numeric value
	char*  err;         // error string
	char*  sym;         // symbol sting
	int    count;       // number of symbols
	struct lval** cell; // self-referential pointer
} lval;

/* Create enumerated types for supported lisp value types */

enum lval_types {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_SEXPR,
	LVAL_QEXPR
};

/* Handle all needed forward declarations */

void lval_print(lval* v);
lval* lval_eval(lval* v);
lval* lval_pop (lval* v, int i);
lval* lval_take (lval* v, int i);
lval* builtin(lval* a, char* func);
lval* builtin_op(lval* a, char* op);
lval* lval_join(lval* x, lval* y);

/* Construct a pointer to a new number type lisp value */

lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num  = x;
	return v;
}

/* Construct a pointer to a new error type lisp value */

lval* lval_err(char* m) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err  = malloc(strlen(m) + 1);
	strcpy(v->err, m);
	return v;
}

/* Construct a pointer to a new symbol type lisp value */

lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym  = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

/* Construct a pointer to a new empty s-expression lisp value */

lval* lval_sexpr(void) {
	lval* v  = malloc(sizeof(lval));
	v->type  = LVAL_SEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

/* Construct a pointer to a new empty q-expression lisp value */

lval* lval_qexpr(void) {
	lval* v  = malloc(sizeof(lval));
	v->type  = LVAL_QEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

/* Destruct a lisp value */

void lval_del(lval* v) {
	switch (v->type) {
		case LVAL_NUM:
			// nothing to free up
			break;
		case LVAL_ERR:
			free(v->err);
			break;
		case LVAL_SYM:
			free(v->sym);
			break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			for (int i = 0; i < v->count; i++) {
				lval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

/* Add a lisp value to an s-expression */

lval* lval_add(lval* v, lval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

/* Read a number lisp value */

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	if (errno != ERANGE) {
		return lval_num(x);
	} else {
		return lval_err("Error: Invalid number");
	}
}

/* Read an expression */

lval* lval_read(mpc_ast_t* t) {

	/* If a symbol or a number, return conversion to that type */

	if (strstr(t->tag, "number")) {
		return lval_read_num(t);
	}

	if (strstr(t->tag, "symbol")) {
		return lval_sym(t->contents);
	}

	/* If the root or an s-expression, then create an empty list */

	lval* x = NULL;

	if ((strcmp(t->tag, ">") == 0) || (strstr(t->tag, "sexpr"))) {
		x = lval_sexpr();
	}

	if (strstr(t->tag, "qexpr")) {
		x = lval_qexpr();
	}

	/* Fill this empty list with any valid expresssions contained wihtin */

	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;

}

/* Print a lisp value expression */

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
		lval_print(v->cell[i]);
		if (i != (v->count - 1)) {
			putchar(' ');
		}
	}
	putchar(close);
}

/* Print a list value */

void lval_print(lval* v) {
	switch (v->type) {
		case LVAL_NUM:
			printf("%li", v->num);
			break;
		case LVAL_ERR:
			printf("Error: %s", v->err);
			break;
		case LVAL_SYM:
			printf("%s", v->sym);
			break;
		case LVAL_SEXPR:
			lval_expr_print(v, '(', ')');
			break;
		case LVAL_QEXPR:
			lval_expr_print(v, '{', '}');
			break;
	}
}

/* Print an lisp value followed by a new line */

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

/* Evalate an s-expresssion */

lval* lval_eval_sexpr(lval* v) {

	/* Evaluate children */

	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}

	/* Error checking */

	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) {
			return lval_take(v, i);
		}
	}

	/* Empty expression */

	if (v->count == 0) {
		return v;
	}

	/* Single expresssion */

	if (v->count == 1) {
		return lval_take(v, 0);
	}

	/* Ensure the first element is a symbol */

	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_SYM) {
		lval_del(f);
		lval_del(v);
		return lval_err("S-expression does not start with a symbol!");
	}

	/* Call built-in operations evaluator with operator */

	lval* result = builtin(v, f->sym);
	lval_del(f);
	return result;

}

/* Evaluate  expressions */

lval* lval_eval(lval* v) {
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(v);
	} else {
		return v;
	}
}

/* Extract a single element from an s-expression */

lval* lval_pop (lval* v, int i) {
	lval* x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}

/* Extract and delete a single element from an s-expression */

lval* lval_take (lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

/* Handle built-in operations */

lval* builtin_op(lval* a, char* op) {

	/* Ensure all arguments are numbers */

	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on a non-number!");
		}
	}

	/* Pop the first element */

	lval* x = lval_pop(a, 0);

	/* If no argument and subtraction operation requested, perform unary negation */

	if ((strcmp(op, "-") == 0) && (a->count == 0)) {
		x->num = -x->num;
	}

	/* Process all remaining elements */

	while (a->count > 0) {

		/* Pop the next element */

		lval* y = lval_pop(a, 0);

		/* Perform the appropriate operation */

		if (strcmp(op, "+") == 0) {
			x->num += y->num;
		}

		if (strcmp(op, "-") == 0) {
			x->num -= y->num;
		}

		if (strcmp(op, "*") == 0) {
			x->num *= y->num;
		}

		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x);
				lval_del(y);
				x = lval_err("Division by zero!");
				break;
			}
			x->num /= y->num;
		}

		if (strcmp(op, "%") == 0) {
			x->num %= y->num;
		}

		/* Delete the element now that we are done with it */

		lval_del(y);

	}

	/* Delete the input expression and return the result */

	lval_del(a);
	return x;

}

/* Handle built-in 'head' function */

lval* builtin_head(lval* a) {

  LASSERT(a, (a->count == 1                 ), "Function 'head' passed too many arguments!");
  LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");
  LASSERT(a, (a->cell[0]->count != 0        ), "Function 'head' passed {}!");

  lval* v = lval_take(a, 0);  
  while (v->count > 1) {
  	lval_del(lval_pop(v, 1));
  }
  return v;
}

/* Handle built-in 'tail' function */

lval* builtin_tail(lval* a) {

	LASSERT(a, (a->count == 1                 ), "Function 'tail' passed too many arguments!");
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");
  	LASSERT(a, (a->cell[0]->count != 0        ), "Function 'tail' passed {}!");

  	lval* v = lval_take(a, 0);  
  	lval_del(lval_pop(v, 0));
  	return v;
}

/* Handle built-in 'list' function */

lval* builtin_list(lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

/* Handle built-in 'eval' function */

lval* builtin_eval(lval* a) {

	LASSERT(a, (a->count == 1                 ), "Function 'eval' passed too many arguments!");
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

  	lval* x = lval_take(a, 0);
  	x->type = LVAL_SEXPR;
  	return lval_eval(x);
}

/* Handle built-in 'join' function */

lval* builtin_join(lval* a) {

	for (int i = 0; i < a->count; i++) {
    	LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type.");
  	}

  	lval* x = lval_pop(a, 0);

  	while (a->count) {
    	x = lval_join(x, lval_pop(a, 0));
  	}

  	lval_del(a);
  	return x;
}

lval* lval_join(lval* x, lval* y) {

  	/* For each cell in 'y' add it to 'x' */

  	while (y->count) {
    	x = lval_add(x, lval_pop(y, 0));
  	}

  	/* Delete the empty 'y' and return 'x' */

  	lval_del(y);  
  	return x;
}

/* Handle built-in 'cons' function */

lval* builtin_cons(lval* a) {

	for (int i = 0; i < a->count; i++) {
    	LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'cons' passed incorrect type.");
  	}

  	lval* x = lval_pop(a, 0);

  	while (a->count) {
    	x = lval_join(x, lval_pop(a, 0));
  	}

  	lval_del(a);
  	return x;
}


lval* builtin(lval* a, char* func) {
  	if (strcmp("list", func) == 0) { return builtin_list(a); }
  	if (strcmp("head", func) == 0) { return builtin_head(a); }
  	if (strcmp("tail", func) == 0) { return builtin_tail(a); }
  	if (strcmp("join", func) == 0) { return builtin_join(a); }
  	if (strcmp("eval", func) == 0) { return builtin_eval(a); }
  	if (strcmp("cons", func) == 0) { return builtin_cons(a); }
  	if (strstr("+-/*", func)) { return builtin_op(a, func); }
  	lval_del(a);
  	return lval_err("Unknown Function!");
}

/* Start REPL */

int main (int argc, char** argv) {

	/* Initialize the mpc parser for Polish Notation */

	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr  = mpc_new("sexpr");
	mpc_parser_t* Qexpr  = mpc_new("qexpr");
	mpc_parser_t* Expr   = mpc_new("expr");
	mpc_parser_t* Lispy  = mpc_new("lispy");

	mpca_lang(MPCA_LANG_DEFAULT,
  		" \
    		number : /-?[0-9]+/ ; \
    		symbol : \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" |'+' | '-' | '*' | '/' | '%' ; \
    		sexpr  : '(' <expr>* ')' ; \
    		qexpr  : '{' <expr>* '}' ; \
    		expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
    		lispy  : /^/ <expr>* /$/ ; \
  		",
  		Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	/* Display Initialization Header */

	puts("Lispy Couch Version 0.0.3");
	puts("Press 'ctrl-c' to exit\n");

	/* Enter into REPL */

	while (true) {

		char* input = readline("lc> ");
		add_history(input);

		mpc_result_t r; if (mpc_parse("<stdin>", input, Lispy, &r)) {
			lval* x = lval_eval(lval_read(r.output));
			lval_println(x);
			lval_del(x);
		}

		free(input);
	}

	/* Clean up and go home now that the hard work is done */

	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	return EXIT_SUCCESS;
}