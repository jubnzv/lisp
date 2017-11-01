#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "./mpc/mpc.h"
#include "lisp.h"

#define L_VERSION "01-11-2017"

#define LASSERT(args, cond, err) \
      if (!(cond)) { lval_del(args); return lval_err(err);  }

int main(int argc, char **argv)
{
    printf("Interpreter version %s\n", L_VERSION);

    /* Define parser */
    mpc_parser_t *Number    = mpc_new("number");
    mpc_parser_t *Symbol    = mpc_new("symbol");
    mpc_parser_t *Sexpr     = mpc_new("sexpr");
    mpc_parser_t *Qexpr     = mpc_new("qexpr");
    mpc_parser_t *Expr      = mpc_new("expr");
    mpc_parser_t *Lispy     = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
            "                                                        \
            number   : /-?[0-9]+/ ;                                  \
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;            \
            sexpr    : '(' <expr>* ')' ;                             \
            qexpr    : '{' <expr>* '}' ;                             \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;     \
            lispy    : /^/ <expr>* /$/ ;                             \
            ",
            Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("C-c for exit.\n");

    /* Initialize new environment */
    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1) {
        char *input = readline("lispy> \n");

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
#ifdef L_DEBUG
			mpc_ast_print(r.output);
#endif

			lval *user_value = lval_read(r.output);
			lval *x = lval_eval(e, user_value);
			lval_println(x);
			lval_del(x);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        add_history(input);

        free(input);
    }

    /* cleanup routines */
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    lenv_del(e);

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

/* Create a new function */
lval *lval_fun(lbuiltin func)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
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

/* Create a pointer to empty qexpr */
lval *lval_qexpr(void)
{
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
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

        case LVAL_FUN:
            break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }

    free(v);
}

/* Initialize a new lisp environment */
lenv *lenv_new(void)
{
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

/* Lisp environment deletion routine */
void *lenv_del(lenv *e)
{
    for (int i = 0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

/* Get value from current env if exists. Otherwise return error. */
lval *lenv_get(lenv *e, lval *v)
{
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], v->sym) == 0)
            return lval_copy(e->vals[i]);
    }

    return lval_err("unbound symbol!");

}

/* Add new entry in Evironment */
void lenv_put(lenv *e, lval *k, lval *v)
{
    /* If variable already exists in Environment we should
     * "redefine" it. */
    for (int i = 0; i < e->count; i++) {
        if(strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            e->syms[i] = realloc(e->syms[i], strlen(k->sym)+1);
            strcpy(e->syms[i], k->sym);
            return;
        }
    }

    /* Initialize new variable otherwise */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval *)*e->count);
    e->syms = realloc(e->syms, sizeof(char *)*e->count);

    e->vals[e->count-1] = lval_copy(v);
    e->syms[e->count-1] = malloc(strlen(k->sym)+1);
    strcpy(e->syms[e->count-1], k->sym);
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
        case LVAL_FUN:
            printf("<function>");
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
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

/* Copy a lisp value */
lval *lval_copy(lval *v)
{
    lval *x = malloc(sizeof(lval));
    x->type = v->type;
    /*printf("TYPE %s\t%s\n", v->sym, ltype_name(v->type));*/

    switch (v->type) {
        case LVAL_FUN:
            x->fun = v->fun;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;
        case LVAL_ERR:
            x->err = malloc(strlen(v->err) + 1);
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            x->sym = malloc(strlen(v->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (int i = 0; i < x->count; i++)
                x->cell[i] = lval_copy(v->cell[i]);
            break;
    }

    return x;
}

/* Reads lisp value from AST */
lval *lval_read(mpc_ast_t *t)
{
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexp then create empty list */
    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0)   { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr"))    { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr"))    { x = lval_qexpr(); }

    /* Fill this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue;  }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue;  }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue;  }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue;  }
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

lval *lval_eval_sexpr(lenv *e, lval *v)
{
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
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

    /* Ensure First Element is a Function */
    lval *first = lval_pop(v, 0);
    if (first->type != LVAL_FUN) {
        lval_del(v); lval_del(first);
        return lval_err("first element is not a function");
    }

    lval* result = first->fun(e, v);
    lval_del(first);
    return result;
}

lval *lval_eval(lenv *e, lval *v)
{
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(e, v);
        lval_del(v);
        return x;
    }

	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }

	return v;
}

lval* builtin_op(lenv *e, lval *a, char *op)
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
	lval* x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1],
			sizeof(lval*) * (v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}

lval *lval_take(lval *v, int i)
{
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;

}


lval *builtin_head(lenv *e, lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect type!");
    LASSERT(a, a->cell[0]->count != 0,
            "Function 'head' passed {}!");

    lval* v = lval_take(a, 0);
    while (v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval *builtin_tail(lenv *e, lval *a)
{
	LASSERT(a, a->count == 1,
			"Function 'tail' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'tail' passed incorrect type!");
	LASSERT(a, a->cell[0]->count != 0,
			"Function 'tail' passed {}!");

	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval *builtin_join(lenv *e, lval *a)
{
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type.");
    }

    lval* x = lval_pop(a, 0);

    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval *builtin_eval(lenv *e, lval *a)
{
    LASSERT(a, a->count == 1,
            "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type!");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval *builtin_list(lenv *e, lval *a)
{
  a->type = LVAL_QEXPR;
  return a;
}

lval* lval_join(lval* x, lval* y)
{

    /* For each cell in 'y' add it to 'x' */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));

    }

    /* Delete the empty 'y' and return 'x' */
    lval_del(y);
    return x;

}

void lenv_add_builtin(lenv *e, char *name, lbuiltin func)
{
    lval *k = lval_sym(name);
    lval *v = lval_fun(func);
    lenv_put(e, k, v);
    lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv *e)
{
    /* List Functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    lenv_add_builtin(e, "def",  builtin_def);
}

lval* builtin_add(lenv *e, lval *a)
{
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv *e, lval *a)
{
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv *e, lval *a)
{
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv *e, lval *a)
{
    return builtin_op(e, a, "/");
}

lval* builtin_def(lenv *e, lval *a)
{
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'def' passed incorrect type!");

    lval* syms = a->cell[0];

    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, syms->cell[i]->type == LVAL_SYM,
                "Function 'def' cannot define non-symbol");
    }

    LASSERT(a, syms->count == a->count-1,
            "Function 'def' cannot define incorrect "
            "number of values to symbols");

    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

char* ltype_name(int t) {
  switch(t) {
    case LVAL_FUN: return "Function";
    case LVAL_NUM: return "Number";
    case LVAL_ERR: return "Error";
    case LVAL_SYM: return "Symbol";
    case LVAL_SEXPR: return "S-Expression";
    case LVAL_QEXPR: return "Q-Expression";
    default: return "Unknown";
  }
}

