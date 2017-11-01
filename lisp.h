#ifndef _LISP_H
#define _LISP_H

#include "./mpc/mpc.h"

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Lisp value */
struct lval {
    int type;
    long num;
    char *err;
    char *sym;
    lbuiltin fun;
    int count;
    struct lval **cell;
};

/* Lisp environment
 * Contains defined names and it values. */
struct lenv {
    int count;
    char **syms;
    lval **vals;
};

/* Types */
enum {
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR,
    LVAL_BIND
};

/* Error codes */
enum {
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

char* ltype_name(int);

lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
lval *lval_read(mpc_ast_t *t);
lval *lval_read_num(mpc_ast_t *t);
lval *lval_add(lval *v, lval *x);
lval *lval_copy(lval *v);

lval *lval_num(long x);
lval *lval_err(char *m);
lval *lval_sym(char *s);
lval *lval_fun(lbuiltin);
lval *lval_sexpr(void);
lval *lval_qexpr(void);
void lval_del(lval *v);

lenv *lenv_new(void);
void *lenv_del(lenv *);
lval *lenv_get(lenv *, lval *);
void lenv_put(lenv *, lval *, lval *);

/* Evaluation operations */
lval* builtin_op(lenv *, lval *, char *);
lval *eval(lval *);
lval *lval_eval_sexpr(lenv *, lval *);
lval *lval_eval(lenv *, lval *);

/* Printing routines */
void lval_print(lval *);
void lval_println(lval *);
void lval_expr_print(lval *v, char open, char close);

/* builtin functions */
void lenv_add_builtin(lenv *, char *, lbuiltin);
void lenv_add_builtins(lenv *);
lval *builtin(lval *, char *);
lval *builtin_add(lenv *, lval *);
lval *builtin_sub(lenv *, lval *);
lval *builtin_mul(lenv *, lval *);
lval *builtin_div(lenv *, lval *);
lval *builtin_head(lenv *, lval *);
lval *builtin_tail(lenv *, lval *);
lval *builtin_join(lenv *, lval *);
lval *builtin_eval(lenv *, lval *);
lval *builtin_list(lenv *, lval *);
lval* builtin_def(lenv *, lval *);
lval *lval_join(lval *, lval *);

#endif /* _LISP_H */
