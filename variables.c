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

#define LASSERT(args, cond, fmt, ...) \
	if (!(cond)) { \
		lval* err = lval_err(fmt, ##__VA_ARGS__); \
		lval_del(args); \
		return err; \
	}

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

/* Handle all needed forward declarations */

struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

void  lval_print(lval* v);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_eval(lenv* e, lval* v);
lval* builtin(lenv* e, lval* a, char* func);
lval* builtin_op(lenv* e, lval* a, char* op);
lval* lval_join(lval* x, lval* y);

/* Declare lbuiltin function pointer */

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare lval structure to carry errors and support multiple types */

struct lval {
	int      type;        // lisp value type
	long     num;         // numeric value
	char*    err;         // error string
	char*    sym;         // symbol sting
	lbuiltin fun;		  // built-in function pointer
	int      count;       // number of symbols
	struct   lval** cell; // self-referential pointer
};

/* Declare enviornment structure to hold defined variables */

struct lenv {
	int 	count;
	char**	syms;
	lval**	vals;
};

/* Create enumerated types for supported lisp value types */

enum lval_types {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_FUN,
	LVAL_SEXPR,
	LVAL_QEXPR
};

/* Construct a pointer to a new number type lisp value */

lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num  = x;
	return v;
}

/* Construct a pointer to a new error type lisp value */

lval* lval_err(char* fmt, ...) {

	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;

	/* Create an initialize a va list */

	va_list va;
	va_start(va, fmt);

	/* Allocate 512 bytes of space for our error message buffer */

	v->err = malloc(512);

	/* printf into the error string with the maximum of 511 characters */

	vsnprintf(v->err, 511, fmt, va);

	/* reallocated the buffer space to the amount really used */

	v->err = realloc(v->err, strlen(v->err)+1);

	/* And, clean up */

	va_end(va);

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

/* Construct a function */

lval* lval_fun(lbuiltin func) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun  = func;
	return v;
}

/* Construct an environment element */

lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms  = NULL;
	e->vals  = NULL;
	return e;
}

/* Destruct a lisp value */

void lval_del(lval* v) {
	switch (v->type) {
		case LVAL_FUN:
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

/* Destruct an environment value */

void lenv_del(lenv* e) {
	for (int i = 0; i < e-> count; i++) {
		free(e->syms[1]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

/* Return a nice name for an argument type */

char* ltype_name(int t) {
	switch(t) {
		case LVAL_FUN:
			return "Function";
		case LVAL_NUM:
			return "Number";
		case LVAL_ERR:
			return "Error";
		case LVAL_SYM:
			return "Symbol";
		case LVAL_SEXPR:
			return "S-Expression";
		case LVAL_QEXPR:
			return "Q-Expression";
		default:
			return "Unknown";
	}
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

/* Copy an lval */

lval* lval_copy(lval* v) {
  
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
  
  	switch (v->type) {
    
    	/* Copy Functions and Numbers Directly */

    	case LVAL_FUN:
    		x->fun = v->fun;
    		break;
    	case LVAL_NUM:
    		x->num = v->num;
    		break;
    
	    /* Copy Strings using malloc and strcpy */

	    case LVAL_ERR:
	    	x->err = malloc(strlen(v->err) + 1);
	    	strcpy(x->err, v->err);
	    	break;
 		case LVAL_SYM:
 			x->sym = malloc(strlen(v->sym) + 1);
 			strcpy(x->sym, v->sym);
 			break;

	    /* Copy Lists by copying each sub-expression */

	    case LVAL_SEXPR:
    	case LVAL_QEXPR:
      		x->count = v->count;
      		x->cell = malloc(sizeof(lval*) * x->count);
      		for (int i = 0; i < x->count; i++) {
        		x->cell[i] = lval_copy(v->cell[i]);
      		}
    		break;
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
		case LVAL_FUN:
			printf("<function>");
			break;
	}
}

/* Print an lisp value followed by a new line */

void lval_println(lval* v) {
	lval_print(v);
	putchar('\n');
}

/* Retreive an environment value */

lval* lenv_get(lenv* e, lval* k) {

	/* Iterate over all items in environment returning a copy of the value if
	 * the stored string matches; otherwise, return an error.
	 */

	for (int i = 0; i < e->count; i++) {
	    if (strcmp(e->syms[i], k->sym) == 0) {
	    	return lval_copy(e->vals[i]);
	    }
	}

	return lval_err("unbound symbol '%s'!", k->sym);

}

void lenv_put(lenv* e, lval* k, lval* v) {

	/* Iterate over all items in environment to see if variable already exists
	 * and if a variable is found, delete the intem in that position replacing it
     * with the variable supplied by the user.
   	 */

   	for (int i = 0; i < e->count; i++) {
    	if (strcmp(e->syms[i], k->sym) == 0) {
      		lval_del(e->vals[i]);
      		e->vals[i] = lval_copy(v);
      		return;
    	}
  	}

	/* If no existing entry found then allocate space for new entry */

	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	/* Copy contents of lval and symbol string into new location */

	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);

}

/* Evalate an s-expresssion */

lval* lval_eval_sexpr(lenv* e, lval* v) {

	/* Evaluate children */

	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
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

	/* Ensure the first element is a fucntion after evaluation */

	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval_del(f);
		lval_del(v);
		return lval_err("First element is not a function");
	}

	/* Call the function to get the result */

	lval* result = f->fun(e, v);
	lval_del(f);

	return result;

}

/* Evaluate expressions according to their type */

lval* lval_eval(lenv* e, lval* v) {
	
	if (v->type == LVAL_SYM) {
		lval* x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	
	if (v->type == LVAL_SEXPR) {
		return lval_eval_sexpr(e, v);
	}

	return v;
/*
	switch (v->type) {
		case LVAL_SYM:
			lval* x = lenv_get(e, v);
			lval_del(v);
			return x;
		case LVAL_SEXPR:
			return lval_eval_sexpr(e, v);
		default:
			return v;
	}
*/
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

lval* builtin_op(lenv* e, lval* a, char* op) {

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

lval* builtin_head(lenv* e, lval* a) {

  	LASSERT(a, (a->count == 1), \
  		"Function 'head' passed too many arguments. Got %i, Expected %i!", a->count, 1);
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'head' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  	LASSERT(a, (a->cell[0]->count != 0), \
  		"Function 'head' passed {}!");

  	lval* v = lval_take(a, 0);  
  	while (v->count > 1) {
  		lval_del(lval_pop(v, 1));
  	}

  	return v;
}

/* Handle built-in 'tail' function */

lval* builtin_tail(lenv* e, lval* a) {

	LASSERT(a, (a->count == 1), \
		"Function 'tail' passed too many arguments. Got %i, Expected %i!", a->count, 1);
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'tail' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  	LASSERT(a, (a->cell[0]->count != 0), \
  		"Function 'tail' passed {}!");

  	lval* v = lval_take(a, 0);  
  	lval_del(lval_pop(v, 0));

  	return v;
}

/* Handle built-in 'list' function */

lval* builtin_list(lenv* e, lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

/* Handle built-in 'eval' function */

lval* builtin_eval(lenv* e, lval* a) {

	LASSERT(a, (a->count == 1), \
		"Function 'eval' passed too many arguments. Got %i, Expected %i!", a->count, 1);
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'eval' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  	lval* x = lval_take(a, 0);
  	x->type = LVAL_SEXPR;

  	return lval_eval(e, x);
}

/* Handle built-in 'join' function */

lval* builtin_join(lenv* e, lval* a) {

	for (int i = 0; i < a->count; i++) {
	  	LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), \
  			"Function 'join' passed incorrect type for argument %i!. Got %s, Expected %s", \
  			i, ltype_name(a->cell[i]->type), ltype_name(LVAL_QEXPR));
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

lval* builtin_cons(lenv* e, lval* a) {

	LASSERT(a, (a->count == 2), \
		"Function 'cons' passed incorrect number of arguments. Got %i, Expected %i", a->count, 2);
  	LASSERT(a, ((a->cell[0]->type == LVAL_QEXPR) || ((a->cell[0]->type == LVAL_NUM))), \
  		"Function 'cons' passed incorrect type for argument 0!. Got %s, Expected %s or %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR), ltype_name(LVAL_NUM));
  	LASSERT(a, (a->cell[1]->type == LVAL_QEXPR), \
  		"Function 'cons' passed incorrect type for argument 1!. Got %s, Expected %s", \
  		ltype_name(a->cell[1]->type), ltype_name(LVAL_QEXPR));

  	lval* x = lval_qexpr();
  	lval_add(x, lval_pop(a, 0));
  	while (a->cell[0]->count) {
  		lval_add(x, lval_pop(a->cell[0],0));
  	}
  	lval_del(a);

  	return x;
}

/* Handle built-in 'len' function */

lval* builtin_len(lenv* e, lval* a) {

	LASSERT(a, (a->count == 1), \
		"Function 'len' passed too many arguments. Got %i, Expected %i", a->count, 1);
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'len' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

  	lval* x = lval_sexpr();
  	lval_add(x, lval_num(a->cell[0]->count));
  	lval_del(a);

  	return lval_eval(e, x);
}

/* Handle built-in 'init' function */

lval* builtin_init(lenv* e, lval* a) {

	LASSERT(a, (a->count == 1), \
		"Function 'init' passed too many arguments. Got %i, Expected %i", a->count, 1);
  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'init' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
  	LASSERT(a, (a->cell[0]->count != 0        ), \
  		"Function 'init' passed {}!");

  	lval* v = lval_qexpr();
	while (a->cell[0]->count > 1) {
  		lval_add(v, lval_pop(a->cell[0], 0));
  	}
  	lval_del(a);

  	return v;

}

/* Provide a built-in function for defining user-defined functions */

lval* builtin_def(lenv* e, lval* a) {

  	LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), \
  		"Function 'def' passed incorrect type for argument 0!. Got %s, Expected %s", \
  		ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));

	/* First argument is symbol list */

	lval* syms = a->cell[0];

	/* Ensure all elements of first list are symbols */

	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, (syms->cell[i]->type == LVAL_SYM), \
			"Function 'def' cannot define non-symbol");
	}

	/* Check correct number of symbols and values */

	LASSERT(a, (syms->count == a->count-1), \
		"Function 'def' cannot define incorrect number of values to symbols");

	/* Assign copies of values to symbols */

	for (int i = 0; i < syms->count; i++) {
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}

	lval_del(a);

	return lval_sexpr();
}

/* Create built-in functions for each of the operators */

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }
lval* builtin_mod(lenv* e, lval* a) { return builtin_op(e, a, "%"); }

/* Dispatch to proper built-in fuction */

lval* builtin(lenv* e, lval* a, char* func) {

  	if (strcmp("list", func) == 0) { return builtin_list(e, a); }
  	if (strcmp("head", func) == 0) { return builtin_head(e, a); }
  	if (strcmp("tail", func) == 0) { return builtin_tail(e, a); }
  	if (strcmp("join", func) == 0) { return builtin_join(e, a); }
  	if (strcmp("eval", func) == 0) { return builtin_eval(e, a); }
  	if (strcmp("cons", func) == 0) { return builtin_cons(e, a); }
  	if (strcmp("len" , func) == 0) { return builtin_len(e, a);  }
  	if (strcmp("init", func) == 0) { return builtin_init(e, a); }
  	if (strstr("+-/*", func))      { return builtin_op(e, a, func); }

  	lval_del(a);

  	return lval_err("Unknown Function!");
}

/* Register a new built-in function with the environment */

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {

	lval* k = lval_sym(name);
	lval* v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k);
	lval_del(v);

}

/* Register all the currently supported built-in fuctions with the environment */

void lenv_add_builtins(lenv* e) {  

	/* List Functions */
	
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "cons", builtin_cons);
	lenv_add_builtin(e, "len",  builtin_len);
	lenv_add_builtin(e, "init", builtin_init);
	lenv_add_builtin(e, "def",  builtin_def);

	/* Mathematical Functions */

	lenv_add_builtin(e, "+",   builtin_add);
	lenv_add_builtin(e, "-",   builtin_sub);
	lenv_add_builtin(e, "*",   builtin_mul);
	lenv_add_builtin(e, "/",   builtin_div);
	lenv_add_builtin(e, "%",   builtin_mod);
	lenv_add_builtin(e, "add", builtin_add);
	lenv_add_builtin(e, "sub", builtin_sub);
	lenv_add_builtin(e, "mul", builtin_mul);
	lenv_add_builtin(e, "div", builtin_div);
	lenv_add_builtin(e, "mod", builtin_mod);

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
    		symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%]+/ ; \
    		sexpr  : '(' <expr>* ')' ; \
    		qexpr  : '{' <expr>* '}' ; \
    		expr   : <number> | <symbol> | <sexpr> | <qexpr> ; \
    		lispy  : /^/ <expr>* /$/ ; \
  		",
  		Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	/* Register built-in functions */

	lenv* e = lenv_new();
	lenv_add_builtins(e);

	/* Display Initialization Header */

	puts("Lispy Couch Version 0.0.3");
	puts("Press 'ctrl-c' to exit\n");

	/* Enter into REPL */

	while (true) {

		char* input = readline("lc> ");
		add_history(input);

		mpc_result_t r;

		if (mpc_parse("<stdin>", input, Lispy, &r)) {
			lval* x = lval_eval(e, lval_read(r.output));
			lval_println(x);
			lval_del(x);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		free(input);
	}

	/* Clean up and go home now that the hard work is done */

	lenv_del(e);
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

	puts("Thank you\n");

	return EXIT_SUCCESS;
}