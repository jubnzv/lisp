#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "./mpc/mpc.h"

#define L_VERSION "0.0.1"


/* =============
 * Type system
 * ============= */

/* Lisp value */
typedef struct lval {
    int type;
    long num;
    char *err;
    char *sym;
    int count;
    struct lval **cell;
} lval;

/* Types */
enum {
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_SEXPR
};

/* Error codes */
enum {
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};


/* =============
 * Function prototypes
 * ============= */
lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *lval_read(mpc_ast_t *t);
lval *lval_read_num(mpc_ast_t *t);
lval *lval_add(lval *v, lval *x);

/* Constructors / destructors */
lval *lval_num(long x);
lval *lval_err(char *m);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
void lval_del(lval *v);

/* Evaluation operations */
lval* builtin_op(lval* a, char* op);
lval *eval(lval *);
lval *lval_eval_sexpr(lval *);
lval *lval_eval(lval *);

/* Printing routines */
void lval_print(lval *);
void lval_println(lval *);
void lval_expr_print(lval *v, char open, char close);

/* ============= */

int main(int argc, char **argv)
{
    static char input[2048]; /* user input */

    printf("Lisp version %s\n", L_VERSION);

    /* Define parser */
    mpc_parser_t *Number    = mpc_new("number");
    mpc_parser_t *Symbol    = mpc_new("symbol");
    mpc_parser_t *Sexpr     = mpc_new("sexpr");
    mpc_parser_t *Expr      = mpc_new("expr");
    mpc_parser_t *Lispy     = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                   \
            number   : /-?[0-9]+/ ;                             \
            symbol   : '+' | '-' | '*' | '/' ;                  \
            sexpr    : '(' <expr>* ')' ;                        \
            expr     : <number> | <symbol> | <sexpr> ;          \
            lispy    : /^/ <expr>* /$/ ;                        \
            ",
            Number, Symbol, Sexpr, Expr, Lispy);

    puts("C-c for exit.\n");

    while(1) {
        char *input = readline("lispy> \n");

        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
#ifdef L_DEBUG
			mpc_ast_print(r.output);
#endif

			lval *user_value = lval_read(r.output);
			lval *x = lval_eval(user_value);
			lval_println(x);
			lval_del(x);
        } else {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        add_history(input);

        free(input);
    }

    /* cleanup routines */
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}


/* Create a new lisp value */
lval *lval_num(long x)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

/* Create a new error */
lval *lval_err(char *m)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m)+1);
    strcpy(v->err, m);
    return v;
}

/* Create a new symbol */
lval *lval_sym(char *s)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s)+1);
    strcpy(v->sym, s);
    return v;
}

/* Create a pointer to empty sexpr */
lval *lval_sexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

/* Destructor */
void lval_del(lval *v)
{
    switch(v->type) {
        case LVAL_NUM:
            break;

        case LVAL_ERR:
            free(v->err);
            break;

        case LVAL_SYM:
            free(v->sym);
            break;
    }
}

/* Prints a lisp value */
void lval_print(lval *v)
{
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
    }
}

void lval_println(lval *v)
{
    lval_print(v);
    putchar('\n');
}

/* Prints a lisp expression */
void lval_expr_print(lval *v, char open, char close)
{
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        /* Don't print trailing space if last element */
        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

/* Reads lisp value from AST */
lval *lval_read(mpc_ast_t *t)
{
#ifdef L_DEBUG
	mpc_ast_print(t);
#endif
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexp then create empty list */
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue;  }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue;  }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue;  }
        x = lval_add(x, lval_read(t->children[i]));
    }

	return x;
}

/* Reads numeric lisp value */
lval *lval_read_num(mpc_ast_t *t)
{
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        lval_num(x) : lval_err("invalid number");
}

/* Add element to S-expression */
lval *lval_add(lval *v, lval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count-1] = x;
    return v;
}

lval *lval_eval_sexpr(lval *v)
{
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Error Checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
			return lval_take(v, i);
		}
    }

    /* Empty Expression */
    if (v->count == 0) { return v; }

    /* Single Expression */
    if (v->count == 1) { return lval_take(v, 0); }

    /* Ensure First Element is Symbol */
    lval *f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        lval_del(f); lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    /* Call builtin with operator */
    lval* result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval* v)
{
	/* Evaluate Sexpressions */
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
	/* All other lval types remain the same */
	return v;
}

lval* builtin_op(lval* a, char* op)
{
	/* Ensure all arguments are numbers */
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}

	/* Pop the first element */
	lval* x = lval_pop(a, 0);

	/* If no arguments and sub then perform unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}

	/* While there are still elements remaining */
	while (a->count > 0) {
		/* Pop the next element */
		lval* y = lval_pop(a, 0);

		if (strcmp(op, "+") == 0) { x->num += y->num;  }
		if (strcmp(op, "-") == 0) { x->num -= y->num;  }
		if (strcmp(op, "*") == 0) { x->num *= y->num;  }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!"); break;
			}
			x->num /= y->num;
		}
		lval_del(y);
	}

	lval_del(a);
	return x;
}

lval *lval_pop(lval *v, int i)
{
	/* Find the item at "i" */
	lval* x = v->cell[i];

	/* Shift memory after the item at "i" over the top */
	memmove(&v->cell[i], &v->cell[i+1],
			sizeof(lval*) * (v->count-i-1));

	/* Decrease the count of items in the list */
	v->count--;

	/* Reallocate the memory used */
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;

}

lval *lval_take(lval *v, int i)
{
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;

}
