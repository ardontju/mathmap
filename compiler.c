/*
 * compiler.c
 *
 * MathMap
 *
 * Copyright (C) 2002 Mark Probst
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#ifndef OPENSTEP
#include <gmodule.h>
#endif

#include "gtypes.h"
#include "mathmap.h"
#include "vars.h"
#include "internals.h"
#include "tags.h"
#include "exprtree.h"
#include "overload.h"
#include "postfix.h"
#include "internals.h"
#include "jump.h"
#include "scanner.h"

/* #define TEST */

#ifdef TEST
void
mathmap_get_pixel (mathmap_invocation_t *invocation, int drawable_index, int frame, int x, int y, guchar *pixel)
{
}
#endif

struct _value_t;

typedef struct
{
    int number;
    int last_index;
} temporary_t;

#define TYPE_INT             1
#define TYPE_FLOAT           2
#define TYPE_COMPLEX         3
#define TYPE_COLOR           4
#define TYPE_MATRIX          5
#define TYPE_VECTOR          6

typedef int type_t;

typedef struct _compvar_t
{
    variable_t *var;		/* 0 if compvar is a temporary */
    temporary_t *temp;		/* 0 if compvar is a variable */
    int n;			/* n/a if compvar is a temporary */
    type_t type;
    struct _value_t *current;
    int flag;
    struct _value_t *values;
} compvar_t;

struct _statement_list_t;

typedef struct _value_t
{
    compvar_t *compvar;
    int index;			/* SSA index */
    struct _statement_list_t *uses;
    struct _value_t *next;	/* next value for same compvar */
} value_t;

#define PRIMARY_VALUE        1
#define PRIMARY_INT_CONST    2
#define PRIMARY_FLOAT_CONST  3

typedef struct
{
    int type;
    union
    {
	value_t *value;
	int int_const;
	float float_const;
    } v;
} primary_t;

#define OP_NOP 0
#define OP_ADD 1
#define OP_SUB 2
#define OP_NEG 3
#define OP_MUL 4
#define OP_DIV 5
#define OP_MOD 6
#define OP_ABS 7
#define OP_MIN 8
#define OP_MAX 9
#define OP_SQRT 10
#define OP_HYPOT 11
#define OP_SIN 12
#define OP_COS 13
#define OP_TAN 14
#define OP_ASIN 15
#define OP_ACOS 16
#define OP_ATAN 17
#define OP_ATAN2 18
#define OP_POW 19
#define OP_EXP 20
#define OP_LOG 21
#define OP_SINH 22
#define OP_COSH 23
#define OP_TANH 24
#define OP_ASINH 25
#define OP_ACOSH 26
#define OP_ATANH 27
#define OP_GAMMA 28
#define OP_FLOOR 29
#define OP_EQ 30
#define OP_LESS 31
#define OP_LEQ 32
#define OP_NOT 33
#define OP_PRINT 34
#define OP_NEWLINE 35
#define OP_ORIG_VAL 36
#define OP_RED 37
#define OP_GREEN 38
#define OP_BLUE 39
#define OP_ALPHA 40
#define OP_COMPLEX 41
#define OP_C_REAL 42
#define OP_C_IMAG 43
#define OP_C_SQRT 44
#define OP_C_SIN 45
#define OP_C_COS 46
#define OP_C_TAN 47
#define OP_C_ASIN 48
#define OP_C_ACOS 49
#define OP_C_ATAN 50
#define OP_C_POW 51
#define OP_C_EXP 52
#define OP_C_LOG 53
#define OP_C_ARG 54
#define OP_C_SINH 55
#define OP_C_COSH 56
#define OP_C_TANH 57
#define OP_C_ASINH 58
#define OP_C_ACOSH 59
#define OP_C_ATANH 60
#define OP_C_GAMMA 61
#define OP_MAKE_M2X2 62
#define OP_MAKE_M3X3 63
#define OP_FREE_MATRIX 64
#define OP_MAKE_V2 65
#define OP_MAKE_V3 66
#define OP_FREE_VECTOR 67
#define OP_VECTOR_NTH 68
#define OP_SOLVE_LINEAR_2 69
#define OP_SOLVE_LINEAR_3 70
#define OP_NOISE 71
#define OP_RAND 72
#define OP_USERVAL_INT 73
#define OP_USERVAL_FLOAT 74
#define OP_USERVAL_BOOL 75
#define OP_USERVAL_CURVE 76
#define OP_USERVAL_COLOR 77
#define OP_USERVAL_GRADIENT 78

#define NUM_OPS             79

#define TYPE_PROP_CONST      1
#define TYPE_PROP_MAX        2

typedef int type_prop_t;

typedef struct _operation_t
{
    char *name;
    int num_args;
    type_prop_t type_prop;
    type_t const_type;		/* used only if type_prop == TYPE_PROP_CONST */
} operation_t;

#define RHS_PRIMARY          1
#define RHS_INTERNAL         2
#define RHS_OP               3

#define MAX_OP_ARGS          9

typedef struct
{
    int type;
    union
    {
	primary_t primary;
	internal_t *internal;
	struct
	{
	    operation_t *op;
	    primary_t args[MAX_OP_ARGS];
	} op;
    } v;
} rhs_t;

#define STMT_NIL             0
#define STMT_ASSIGN          1
#define STMT_PHI_ASSIGN      2
#define STMT_IF_COND         3
#define STMT_WHILE_LOOP      4

typedef struct _statement_t
{
    int type;
    int index;
    union
    {
	struct
	{
	    value_t *lhs;
	    rhs_t *rhs;
	    rhs_t *rhs2;	/* only valid for STMT_PHI_ASSIGN */
	    value_t *old_value;	/* only valid for STMT_PHI_ASSIGN */
	} assign;
	struct
	{
	    rhs_t *condition;
	    struct _statement_t *consequent;
	    struct _statement_t *alternative;
	} if_cond;
	struct
	{
	    struct _statement_t *entry;
	    rhs_t *invariant;
	    struct _statement_t *body;
	} while_loop;
    } v;
    struct _statement_t *next;
} statement_t;

typedef struct _statement_list_t
{
    statement_t *stmt;
    struct _statement_list_t *next;
} statement_list_t;

static operation_t ops[NUM_OPS];

static int next_stmt_index = 0;
static int next_temp_number = 1;

static statement_t *first_stmt = 0;
static statement_t **emit_loc = &first_stmt;

#define STMT_STACK_SIZE            64

static statement_t *stmt_stack[STMT_STACK_SIZE];
static int stmt_stackp = 0;

#define alloc_stmt()               ((statement_t*)malloc(sizeof(statement_t)))
#define alloc_value()              ((value_t*)malloc(sizeof(value_t)))
#define alloc_rhs()                ((rhs_t*)malloc(sizeof(rhs_t)))
#define alloc_compvar()            (compvar_t*)malloc(sizeof(compvar_t))

compvar_t*
make_temporary (void)
{
    temporary_t *temp = (temporary_t*)malloc(sizeof(temporary_t));
    compvar_t *compvar = alloc_compvar();
    value_t *val = alloc_value();

    temp->number = next_temp_number++;
    temp->last_index = 0;

    compvar->var = 0;
    compvar->temp = temp;
    compvar->type = TYPE_INT;
    compvar->current = val;
    compvar->flag = 0;
    compvar->values = val;

    val->compvar = compvar;	/* dummy value */
    val->index = -1;
    val->uses = 0;
    val->next = 0;

    return compvar;
}

compvar_t*
make_variable (variable_t *var, int n)
{
    compvar_t *compvar = alloc_compvar();
    value_t *val = alloc_value();

    compvar->var = var;
    compvar->temp = 0;
    compvar->n = n;
    compvar->type = TYPE_INT;
    compvar->current = val;
    compvar->flag = 0;
    compvar->values = val;

    val->compvar = compvar;
    val->index = -1;
    val->uses = 0;
    val->next = 0;

    return compvar;
}

value_t*
make_lhs (compvar_t *compvar)
{
    value_t *val = alloc_value();

    val->compvar = compvar;
    val->index = -1;
    val->uses = 0;

    val->next = compvar->values;
    compvar->values = val;

    return val;
}

statement_list_t*
prepend_statement (statement_t *stmt, statement_list_t *rest)
{
    statement_list_t *lst = (statement_list_t*)malloc(sizeof(statement_list_t));

    lst->stmt = stmt;
    lst->next = rest;

    return lst;
}

void
add_use (value_t *val, statement_t *stmt)
{
    val->uses = prepend_statement(stmt, val->uses);
}

void
remove_use (value_t *val, statement_t *stmt)
{
    statement_list_t **lst = &val->uses;

    while (*lst != 0)
    {
	statement_list_t *elem = *lst;

	if (elem->stmt == stmt)
	{
	    *lst = elem->next;

	    free(elem);

	    return;
	}

	lst = &(*lst)->next;
    }

    assert(0);
}

value_t*
make_value_copy (value_t *val)
{
    return make_lhs(val->compvar);
}

void
set_value_current (value_t *val, value_t *new_current)
{
    val->compvar->current = new_current;
}

value_t*
current_value (compvar_t *compvar)
{
    return compvar->current;
}

void
assign_value_index_and_make_current (value_t *val)
{
    if (val->compvar->var != 0)
	val->index = ++val->compvar->var->last_index[val->compvar->n];
    else
	val->index = ++val->compvar->temp->last_index;

    set_value_current(val, val);
}

primary_t
make_int_const_primary (int int_const)
{
    primary_t primary;

    primary.type = PRIMARY_INT_CONST;
    primary.v.int_const = int_const;

    return primary;
}

primary_t
make_compvar_primary (compvar_t *compvar)
{
    primary_t primary;

    primary.type = PRIMARY_VALUE;
    primary.v.value = current_value(compvar);

    return primary;
}

rhs_t*
make_int_const_rhs (int int_const)
{
    rhs_t *rhs = alloc_rhs();

    rhs->type = RHS_PRIMARY;
    rhs->v.primary.type = PRIMARY_INT_CONST;
    rhs->v.primary.v.int_const = int_const;

    return rhs;
}

rhs_t*
make_float_const_rhs (float float_const)
{
    rhs_t *rhs = alloc_rhs();

    rhs->type = RHS_PRIMARY;
    rhs->v.primary.type = PRIMARY_FLOAT_CONST;
    rhs->v.primary.v.float_const = float_const;

    return rhs;
}

rhs_t*
make_value_rhs (value_t *val)
{
    rhs_t *rhs = alloc_rhs();

    assert(val != 0);

    rhs->type = RHS_PRIMARY;
    rhs->v.primary.type = PRIMARY_VALUE;
    rhs->v.primary.v.value = val;

    return rhs;
}

rhs_t*
make_compvar_rhs (compvar_t *compvar)
{
    return make_value_rhs(current_value(compvar));
}

rhs_t*
make_internal_rhs (internal_t *internal)
{
    rhs_t *rhs = alloc_rhs();

    rhs->type = RHS_INTERNAL;
    rhs->v.internal = internal;

    return rhs;
}

rhs_t*
make_op_rhs (int op_index, ...)
{
    rhs_t *rhs = alloc_rhs();
    va_list ap;
    int i;

    rhs->type = RHS_OP;
    rhs->v.op.op = &ops[op_index];

    va_start(ap, op_index);
    for (i = 0; i < ops[op_index].num_args; ++i)
	rhs->v.op.args[i] = va_arg(ap, primary_t);
    va_end(ap);

    return rhs;
}

void
free_rhs (rhs_t *rhs)
{
    /* FIXME */
}

statement_t*
find_phi_assign (statement_t *stmts, compvar_t *compvar)
{
    for (; stmts != 0; stmts = stmts->next)
    {
	assert(stmts->type == STMT_PHI_ASSIGN);

	if (stmts->v.assign.lhs->compvar == compvar)
	    return stmts;
    }

    return 0;
}

primary_t*
find_value_in_rhs (value_t *val, rhs_t *rhs)
{
    switch (rhs->type)
    {
	case RHS_PRIMARY :
	    if (rhs->v.primary.type == PRIMARY_VALUE
		&& rhs->v.primary.v.value == val)
		return &rhs->v.primary;
	    else
		return 0;
	    break;

	case RHS_INTERNAL :
	    return 0;

	case RHS_OP :
	    {
		int i;

		for (i = 0; i < rhs->v.op.op->num_args; ++i)
		    if (rhs->v.op.args[i].type == PRIMARY_VALUE
			&& rhs->v.op.args[i].v.value == val)
			return &rhs->v.op.args[i];
		return 0;
	    }
	    break;
    }

    return 0;
}

void
rewrite_uses (value_t *old, value_t *new, int start_index)
{
    statement_list_t **lst;

    assert(old != new);

    lst = &old->uses;
    while (*lst != 0)
    {
	statement_list_t *elem = *lst;
	statement_t *stmt = elem->stmt;
	primary_t *primary;

	if (stmt->index >= start_index)
	{
	    switch (stmt->type)
	    {
		case STMT_ASSIGN :
		    primary = find_value_in_rhs(old, stmt->v.assign.rhs);
		    break;

		case STMT_PHI_ASSIGN :
		    primary = find_value_in_rhs(old, stmt->v.assign.rhs);
		    if (primary == 0)
			primary = find_value_in_rhs(old, stmt->v.assign.rhs2);
		    break;

		case STMT_IF_COND :
		    primary = find_value_in_rhs(old, stmt->v.if_cond.condition);
		    break;

		case STMT_WHILE_LOOP :
		    primary = find_value_in_rhs(old, stmt->v.while_loop.invariant);
		    break;

		default :
		    assert(0);
	    }

	    assert(primary != 0 && primary->v.value == old);

	    primary->v.value = new;
	    add_use(new, stmt);

	    *lst = elem->next;

	    free(elem);
	}
	else
	    lst = &elem->next;
    }
}

void
commit_assign (statement_t *stmt)
{
    statement_t *tos;

    if (stmt_stackp > 0)
    {
	tos = stmt_stack[stmt_stackp - 1];

	switch (tos->type)
	{
	    case STMT_IF_COND :
		{
		    statement_t *phi_assign = find_phi_assign(tos->next, stmt->v.assign.lhs->compvar);

		    if (phi_assign == 0)
		    {
			phi_assign = alloc_stmt();

			phi_assign->type = STMT_PHI_ASSIGN;

			phi_assign->v.assign.lhs = make_value_copy(stmt->v.assign.lhs);

			phi_assign->v.assign.rhs = make_value_rhs(current_value(stmt->v.assign.lhs->compvar));
			add_use(current_value(stmt->v.assign.lhs->compvar), phi_assign);

			phi_assign->v.assign.rhs2 = make_value_rhs(current_value(stmt->v.assign.lhs->compvar));
			add_use(current_value(stmt->v.assign.lhs->compvar), phi_assign);

			phi_assign->v.assign.old_value = current_value(stmt->v.assign.lhs->compvar);

			phi_assign->next = tos->next;
			tos->next = phi_assign;
		    }

		    if (tos->v.if_cond.alternative == 0)
		    {
			assert(phi_assign->v.assign.rhs->type = RHS_PRIMARY
			       && phi_assign->v.assign.rhs->v.primary.type == PRIMARY_VALUE);
			remove_use(phi_assign->v.assign.rhs->v.primary.v.value, phi_assign);
			free_rhs(phi_assign->v.assign.rhs);

			phi_assign->v.assign.rhs = make_value_rhs(stmt->v.assign.lhs);
			add_use(stmt->v.assign.lhs, phi_assign);
		    }
		    else
		    {
			assert(phi_assign->v.assign.rhs2->type = RHS_PRIMARY
			       && phi_assign->v.assign.rhs2->v.primary.type == PRIMARY_VALUE);
			remove_use(phi_assign->v.assign.rhs2->v.primary.v.value, phi_assign);
			free_rhs(phi_assign->v.assign.rhs2);

			phi_assign->v.assign.rhs2 = make_value_rhs(stmt->v.assign.lhs);
			add_use(stmt->v.assign.lhs, phi_assign);
		    }
		}
		break;

	    case STMT_WHILE_LOOP :
		{
		    statement_t *phi_assign = find_phi_assign(tos->v.while_loop.entry, stmt->v.assign.lhs->compvar);

		    if (phi_assign == 0)
		    {
			phi_assign = alloc_stmt();

			phi_assign->type = STMT_PHI_ASSIGN;

			phi_assign->v.assign.lhs = make_value_copy(stmt->v.assign.lhs);

			phi_assign->v.assign.rhs = make_value_rhs(current_value(stmt->v.assign.lhs->compvar));
			add_use(current_value(stmt->v.assign.lhs->compvar), phi_assign);

			phi_assign->next = tos->v.while_loop.entry;
			tos->v.while_loop.entry = phi_assign;
		    }
		    else
		    {
			assert(phi_assign->v.assign.rhs2->type = RHS_PRIMARY
			       && phi_assign->v.assign.rhs2->v.primary.type == PRIMARY_VALUE);
			remove_use(phi_assign->v.assign.rhs2->v.primary.v.value, phi_assign);
			free_rhs(phi_assign->v.assign.rhs2);
		    }

		    phi_assign->v.assign.rhs2 = make_value_rhs(stmt->v.assign.lhs);
		    add_use(stmt->v.assign.lhs, phi_assign);

		    rewrite_uses(current_value(stmt->v.assign.lhs->compvar), phi_assign->v.assign.lhs, tos->index);
		}
		break;

	    default :
		assert(0);
	}
    }

    assign_value_index_and_make_current(stmt->v.assign.lhs);
}

void
emit_stmt (statement_t *stmt)
{
    stmt->index = next_stmt_index++;

    *emit_loc = stmt;
    emit_loc = &stmt->next;

    switch (stmt->type)
    {
	case STMT_ASSIGN :
	    if (stmt->v.assign.rhs->type == RHS_PRIMARY
		&& stmt->v.assign.rhs->v.primary.type == PRIMARY_VALUE)
		add_use(stmt->v.assign.rhs->v.primary.v.value, stmt);
	    else if (stmt->v.assign.rhs->type == RHS_OP)
	    {
		int i;

		for (i = 0; i < stmt->v.assign.rhs->v.op.op->num_args; ++i)
		    if (stmt->v.assign.rhs->v.op.args[i].type == PRIMARY_VALUE)
			add_use(stmt->v.assign.rhs->v.op.args[i].v.value, stmt);
	    }
	    break;

	case STMT_PHI_ASSIGN :
	    if (stmt->v.assign.rhs->type == RHS_PRIMARY
		&& stmt->v.assign.rhs->v.primary.type == PRIMARY_VALUE)
		add_use(stmt->v.assign.rhs->v.primary.v.value, stmt);
	    if (stmt->v.assign.rhs2->type == RHS_PRIMARY
		&& stmt->v.assign.rhs2->v.primary.type == PRIMARY_VALUE)
		add_use(stmt->v.assign.rhs2->v.primary.v.value, stmt);
	    break;

	case STMT_IF_COND :
	    if (stmt->v.if_cond.condition->type == RHS_PRIMARY
		&& stmt->v.if_cond.condition->v.primary.type == PRIMARY_VALUE)
		add_use(stmt->v.if_cond.condition->v.primary.v.value, stmt);
	    break;

	case STMT_WHILE_LOOP :
	    if (stmt->v.while_loop.invariant->type == RHS_PRIMARY
		&& stmt->v.while_loop.invariant->v.primary.type == PRIMARY_VALUE)
		add_use(stmt->v.while_loop.invariant->v.primary.v.value, stmt);
	    break;
    }
}

void
emit_nil (void)
{
    statement_t *stmt = alloc_stmt();

    stmt->type = STMT_NIL;
    stmt->next = 0;

    emit_stmt(stmt);
}

void
emit_assign (value_t *lhs, rhs_t *rhs)
{
    statement_t *stmt = alloc_stmt();

    stmt->type = STMT_ASSIGN;
    stmt->next = 0;

    stmt->v.assign.lhs = lhs;
    stmt->v.assign.rhs = rhs;

    emit_stmt(stmt);

    commit_assign(stmt);
}

void
start_if_cond (rhs_t *condition)
{
    statement_t *stmt = alloc_stmt();

    stmt->type = STMT_IF_COND;
    stmt->next = 0;

    stmt->v.if_cond.condition = condition;
    stmt->v.if_cond.consequent = 0;
    stmt->v.if_cond.alternative = 0;

    *emit_loc = stmt;
    stmt_stack[stmt_stackp++] = stmt;

    emit_loc = &stmt->v.if_cond.consequent;
}

void
switch_if_branch (void)
{
    statement_t *stmt, *phi;

    assert(stmt_stackp > 0);

    stmt = stmt_stack[stmt_stackp - 1];

    assert(stmt->type == STMT_IF_COND && stmt->v.if_cond.alternative == 0);

    if (stmt->v.if_cond.consequent == 0)
	emit_nil();

    for (phi = stmt->next; phi != 0; phi = phi->next)
    {
	assert(phi->type == STMT_PHI_ASSIGN);

	set_value_current(phi->v.assign.lhs, phi->v.assign.old_value);

	phi->v.assign.old_value = 0;
    }

    emit_loc = &stmt->v.if_cond.alternative;
}

void
end_if_cond (void)
{
    statement_t *stmt, *phi;

    assert(stmt_stackp > 0);

    stmt = stmt_stack[--stmt_stackp];

    assert(stmt->type == STMT_IF_COND && stmt->v.if_cond.consequent != 0);

    if (stmt->v.if_cond.alternative == 0)
	emit_nil();

    for (phi = stmt->next; phi != 0; phi = phi->next)
    {
	assert(phi->type == STMT_PHI_ASSIGN);

	commit_assign(phi);

	if (phi->next == 0)
	    stmt = phi;
    }

    emit_loc = &stmt->next;
}

void
start_while_loop (rhs_t *invariant)
{
    statement_t *stmt = alloc_stmt();

    stmt->type = STMT_WHILE_LOOP;
    stmt->next = 0;

    stmt->v.while_loop.entry = 0;
    stmt->v.while_loop.invariant = invariant;
    stmt->v.while_loop.body = 0;

    emit_stmt(stmt);
    stmt_stack[stmt_stackp++] = stmt;

    emit_loc = &stmt->v.while_loop.body;
}

void
end_while_loop (void)
{
    statement_t *stmt, *phi;

    assert(stmt_stackp > 0);

    stmt = stmt_stack[--stmt_stackp];

    assert(stmt->type == STMT_WHILE_LOOP);

    if (stmt->v.while_loop.body == 0)
	emit_nil();

    for (phi = stmt->v.while_loop.entry; phi != 0; phi = phi->next)
    {
	assert(phi->type == STMT_PHI_ASSIGN);

	commit_assign(phi);
    }

    emit_loc = &stmt->next;
}

#include "new_builtins.c"

void
print_indent (int indent)
{
    int i;

    for (i = 0; i < indent; ++i)
	fputs("  ", stdout);
}

void
print_value (value_t *val)
{
    if (val->compvar->var != 0)
	printf("%s[%d]_%d", val->compvar->var->name, val->compvar->n, val->index);
    else
	printf("$t%d_%d", val->compvar->temp->number, val->index);
}

void
print_primary (primary_t *primary)
{
    switch (primary->type)
    {
	case PRIMARY_VALUE :
	    print_value(primary->v.value);
	    break;

	case PRIMARY_INT_CONST :
	    printf("%d", primary->v.int_const);
	    break;

	case PRIMARY_FLOAT_CONST :
	    printf("%f", primary->v.float_const);
	    break;

	default :
	    assert(0);
    }
}

void
print_rhs (rhs_t *rhs)
{
    switch (rhs->type)
    {
	case RHS_PRIMARY :
	    print_primary(&rhs->v.primary);
	    break;

	case RHS_INTERNAL :
	    printf("%s", rhs->v.internal->name);
	    break;

	case RHS_OP :
	    {
		int i;

		printf("%s", rhs->v.op.op->name);
		for (i = 0; i < rhs->v.op.op->num_args; ++i)
		{
		    printf(" ");
		    print_primary(&rhs->v.op.args[i]);
		}
	    }
	    break;

	    /*
	case RHS_INT_CONST :
	    printf("%d", rhs->v.int_const);
	    break;

	case RHS_FLOAT_CONST :
	    printf("%f", rhs->v.float_const);
	    break;
	    */

	default :
	    assert(0);
    }
}

void
dump_code (statement_t *stmt, int indent)
{
    while (stmt != 0)
    {
	switch (stmt->type)
	{
	    case STMT_NIL :
		print_indent(indent);
		printf("nil\n");
		break;

	    case STMT_ASSIGN :
		print_indent(indent);
		print_value(stmt->v.assign.lhs);
		printf(" = ");
		print_rhs(stmt->v.assign.rhs);
		printf("\n");
		break;

	    case STMT_PHI_ASSIGN :
		print_indent(indent);
		print_value(stmt->v.assign.lhs);
		printf(" = phi(");
		print_rhs(stmt->v.assign.rhs);
		printf(", ");
		print_rhs(stmt->v.assign.rhs2);
		printf(")\n");
		break;

	    case STMT_IF_COND :
		print_indent(indent);
		printf("if ");
		print_rhs(stmt->v.if_cond.condition);
		printf("\n");
		dump_code(stmt->v.if_cond.consequent, indent + 1);
		print_indent(indent);
		printf("else\n");
		dump_code(stmt->v.if_cond.alternative, indent + 1);
		break;

	    case STMT_WHILE_LOOP :
		print_indent(indent);
		printf("start while\n");
		dump_code(stmt->v.while_loop.entry, indent + 1);
		print_indent(indent);
		printf("while ");
		print_rhs(stmt->v.while_loop.invariant);
		printf("\n");
		dump_code(stmt->v.while_loop.body, indent + 1);
		break;

	    default :
		assert(0);
	}

	stmt = stmt->next;
    }
}

static void
alloc_var_compvars_if_needed (variable_t *var)
{
    int i;

    for (i = 0; i < var->type.length; ++i)
	if (var->compvar[i] == 0)
	    var->compvar[i] = make_variable(var, i);
}

static void
gen_code (exprtree *tree, compvar_t **dest, int is_alloced)
{
    int i;

    switch (tree->type)
    {
	case EXPR_INT_CONST :
	    if (!is_alloced)
		dest[0] = make_temporary();
	    emit_assign(make_lhs(dest[0]), make_int_const_rhs(tree->val.int_const));
	    break;

	case EXPR_FLOAT_CONST :
	    if (!is_alloced)
		dest[0] = make_temporary();
	    emit_assign(make_lhs(dest[0]), make_float_const_rhs(tree->val.float_const));
	    break;

	case EXPR_TUPLE_CONST :
	    for (i = 0; i < tree->val.tuple_const.length; ++i)
	    {
		if (!is_alloced)
		    dest[i] = make_temporary();
		emit_assign(make_lhs(dest[i]), make_float_const_rhs(tree->val.tuple_const.data[i]));
	    }
	    break;

	case EXPR_TUPLE :
	    {
		exprtree *elem;

		for (i = 0, elem = tree->val.tuple.elems; elem != 0; ++i, elem = elem->next)
		    gen_code(elem, dest + i, is_alloced);
	    }
	    break;

	case EXPR_SELECT :
	    {
		compvar_t *temps[tree->val.select.tuple->result.length];
		exprtree *sub;
		int i;

		gen_code(tree->val.select.tuple, temps, 0);

		for (sub = tree->val.select.subscripts, i = 0; sub != 0; sub = sub->next, ++i)
		{
		    int subscript;

		    if (is_exprtree_single_const(sub, &subscript, 0))
		    {
			if (subscript < 0)
			    subscript = 0;
			if (subscript >= tree->val.select.tuple->result.length)
			    subscript = tree->val.select.tuple->result.length - 1;

			if (!is_alloced)
			    dest[i] = temps[subscript];
			else
			    emit_assign(make_lhs(dest[i]), make_compvar_rhs(temps[subscript]));
		    }
		    else
		    {
			compvar_t *subscript;
			int length = tree->val.select.tuple->result.length;
			int j;

			if (!is_alloced)
			    dest[i] = make_temporary();

			gen_code(sub, &subscript, 0);

			for (j = 1; j < length; ++j)
			{
			    start_if_cond(make_op_rhs(OP_LESS, make_compvar_primary(subscript), make_int_const_primary(j)));
			    emit_assign(make_lhs(dest[i]), make_compvar_rhs(temps[j - 1]));
			    switch_if_branch();
			}
			emit_assign(make_lhs(dest[i]), make_compvar_rhs(temps[length - 1]));
			for (j = 0; j < length - 1; ++j)
			    end_if_cond();
		    }
		}
	    }
	    break;

	case EXPR_VARIABLE :
	    alloc_var_compvars_if_needed(tree->val.var);
	    for (i = 0; i < tree->val.var->type.length; ++i)
		if (!is_alloced)
		    dest[i] = tree->val.var->compvar[i];
		else
		    emit_assign(make_lhs(dest[i]), make_compvar_rhs(tree->val.var->compvar[i]));
	    break;

	case EXPR_INTERNAL :
	    if (!is_alloced)
		dest[0] = make_temporary();
	    emit_assign(make_lhs(dest[0]), make_internal_rhs(tree->val.internal));
	    break;

	case EXPR_ASSIGNMENT :
	    alloc_var_compvars_if_needed(tree->val.assignment.var);
	    gen_code(tree->val.assignment.value, tree->val.assignment.var->compvar, 1);
	    for (i = 0; i < tree->result.length; ++i)
		if (is_alloced)
		    emit_assign(make_lhs(dest[i]), make_compvar_rhs(tree->val.assignment.var->compvar[i]));
		else
		    dest[i] = tree->val.assignment.var->compvar[i];
	    break;

	case EXPR_SUB_ASSIGNMENT :
	    {
		compvar_t *temps[tree->val.sub_assignment.value->result.length];
		exprtree *sub;
		int i;

		alloc_var_compvars_if_needed(tree->val.sub_assignment.var);

		gen_code(tree->val.sub_assignment.value, temps, 0);

		for (sub = tree->val.sub_assignment.subscripts, i = 0; sub != 0; sub = sub->next, ++i)
		{
		    int subscript;
		    
		    if (is_exprtree_single_const(sub, &subscript, 0))
		    {
			if (subscript < 0)
			    subscript = 0;
			if (subscript >= tree->result.length)
			    subscript = tree->result.length - 1;

			emit_assign(make_lhs(tree->val.sub_assignment.var->compvar[subscript]), make_compvar_rhs(temps[i]));
		    }
		    else
		    {
			compvar_t *subscript;
			int length = tree->val.sub_assignment.var->type.length;
			int j;

			if (!is_alloced)
			    dest[i] = make_temporary();

			gen_code(sub, &subscript, 0);

			for (j = 1; j < length; ++j)
			{
			    start_if_cond(make_op_rhs(OP_LESS, make_compvar_primary(subscript), make_int_const_primary(j)));
			    emit_assign(make_lhs(tree->val.sub_assignment.var->compvar[j - 1]), make_compvar_rhs(temps[i]));
			    switch_if_branch();
			}
			emit_assign(make_lhs(tree->val.sub_assignment.var->compvar[length - 1]), make_compvar_rhs(temps[i]));
			for (j = 0; j < length - 1; ++j)
			    end_if_cond();
		    }

		    if (is_alloced)
			emit_assign(make_lhs(dest[i]), make_compvar_rhs(temps[i]));
		    else
			dest[i] = temps[i];
		}
	    }
	    break;

	case EXPR_CAST :
	    gen_code(tree->val.cast.tuple, dest, is_alloced);
	    break;

	case EXPR_FUNC :
	    {
		exprtree *arg;
		int num_args = 0;
		compvar_t ***args;
		int *arglengths;

		for (arg = tree->val.func.args; arg != 0; arg = arg->next)
		    ++num_args;

		args = (compvar_t***)malloc(num_args * sizeof(compvar_t**));
		arglengths = (int*)malloc(num_args * sizeof(int));

		for (i = 0, arg = tree->val.func.args; i < num_args; ++i, arg = arg->next)
		{
		    args[i] = (compvar_t**)malloc(arg->result.length * sizeof(compvar_t*));
		    arglengths[i] = arg->result.length;
		    gen_code(arg, args[i], 0);
		}

		if (!is_alloced)
		    for (i = 0; i < tree->result.length; ++i)
			dest[i] = make_temporary();

		tree->val.func.entry->v.builtin.generator(args, arglengths, dest);

		for (i = 0; i < num_args; ++i)
		    free(args[i]);
		free(arglengths);
		free(args);
	    }
	    break;
 
	case EXPR_SEQUENCE :
	    {
		compvar_t **left_result;

		left_result = (compvar_t**)malloc(tree->val.operator.left->result.length * sizeof(compvar_t*));
		gen_code(tree->val.operator.left, left_result, 0);
		free(left_result);

		gen_code(tree->val.operator.right, dest, is_alloced);
	    }
	    break;

	case EXPR_IF_THEN :
	case EXPR_IF_THEN_ELSE :
	    {
		compvar_t *condition;
		compvar_t **result = (compvar_t**)malloc(tree->result.length * sizeof(compvar_t*));

		for (i = 0; i < tree->result.length; ++i)
		    result[i] = make_temporary();

		gen_code(tree->val.ifExpr.condition, &condition, 0);

		start_if_cond(make_compvar_rhs(condition));

		gen_code(tree->val.ifExpr.consequent, result, 1);

		switch_if_branch();

		if (tree->type == EXPR_IF_THEN_ELSE)
		    gen_code(tree->val.ifExpr.alternative, result, 1);

		end_if_cond();

		for (i = 0; i < tree->result.length; ++i)
		    if (is_alloced)
			emit_assign(make_lhs(dest[i]), make_compvar_rhs(result[i]));
		    else
			dest[i] = result[i];

		free(result);
	    }
	    break;

	case EXPR_DO_WHILE :
	case EXPR_WHILE :
	    {
		compvar_t *invariant = make_temporary();
		compvar_t **body_result = (compvar_t**)malloc(tree->val.whileExpr.body->result.length * sizeof(compvar_t*));

		if (tree->type == EXPR_DO_WHILE)
		    gen_code(tree->val.whileExpr.body, body_result, 0);

		gen_code(tree->val.whileExpr.invariant, &invariant, 1);
		start_while_loop(make_compvar_rhs(invariant));
		gen_code(tree->val.whileExpr.body, body_result, 0);
		gen_code(tree->val.whileExpr.invariant, &invariant, 1);
		end_while_loop();

		free(body_result);

		if (!is_alloced)
		    dest[0] = make_temporary();
		emit_assign(make_lhs(dest[0]), make_int_const_rhs(0));
	    }
	    break;

	case EXPR_USERVAL :
	    switch (tree->val.userval.info->type)
	    {
		case USERVAL_INT_CONST :
		    if (!is_alloced)
			dest[0] = make_temporary();
		    emit_assign(make_lhs(dest[0]), make_op_rhs(OP_USERVAL_INT, make_int_const_primary(tree->val.userval.info->index)));
		    break;

		case USERVAL_FLOAT_CONST :
		    if (!is_alloced)
			dest[0] = make_temporary();
		    emit_assign(make_lhs(dest[0]), make_op_rhs(OP_USERVAL_FLOAT, make_int_const_primary(tree->val.userval.info->index)));
		    break;

		case USERVAL_BOOL_CONST :
		    if (!is_alloced)
			dest[0] = make_temporary();
		    emit_assign(make_lhs(dest[0]), make_op_rhs(OP_USERVAL_BOOL, make_int_const_primary(tree->val.userval.info->index)));
		    break;

		case USERVAL_CURVE :
		    {
			compvar_t *pos;

			if (!is_alloced)
			    dest[0] = make_temporary();

			gen_code(tree->val.userval.args, &pos, 0);

			emit_assign(make_lhs(dest[0]), make_op_rhs(OP_USERVAL_CURVE, make_int_const_primary(tree->val.userval.info->index),
								   make_compvar_primary(pos)));
		    }
		    break;

		case USERVAL_COLOR :
		case USERVAL_GRADIENT :
		    {
			compvar_t *pos;
			compvar_t *temp = make_temporary();
			int i;

			if (!is_alloced)
			    for (i = 0; i < 4; ++i)
				dest[i] = make_temporary();

			if (tree->val.userval.info->type == USERVAL_COLOR)
			    emit_assign(make_lhs(temp), make_op_rhs(OP_USERVAL_COLOR, make_int_const_primary(tree->val.userval.info->index)));
			else
			{
			    gen_code(tree->val.userval.args, &pos, 0);
			    emit_assign(make_lhs(temp), make_op_rhs(OP_USERVAL_GRADIENT, make_int_const_primary(tree->val.userval.info->index),
								    make_compvar_primary(pos)));
			}

			emit_assign(make_lhs(dest[0]), make_op_rhs(OP_RED, make_compvar_primary(temp)));
			emit_assign(make_lhs(dest[1]), make_op_rhs(OP_GREEN, make_compvar_primary(temp)));
			emit_assign(make_lhs(dest[2]), make_op_rhs(OP_BLUE, make_compvar_primary(temp)));
			emit_assign(make_lhs(dest[3]), make_op_rhs(OP_ALPHA, make_compvar_primary(temp)));
		    }
		    break;

		case USERVAL_IMAGE :
		    if (!is_alloced)
			dest[0] = make_temporary();
		    emit_assign(make_lhs(dest[0]), make_int_const_rhs(tree->val.userval.info->index));
		    break;

		default :
		    assert(0);
	    }
	    break;

	default :
	    assert(0);
   }
}

type_t
primary_type (primary_t *primary)
{
    switch (primary->type)
    {
	case PRIMARY_VALUE :
	    return primary->v.value->compvar->type;

	case PRIMARY_INT_CONST :
	    return TYPE_INT;

	case PRIMARY_FLOAT_CONST :
	    return TYPE_FLOAT;

	default :
	    assert(0);
    }
}

type_t
rhs_type (rhs_t *rhs)
{
    switch (rhs->type)
    {
	case RHS_PRIMARY :
	    return primary_type(&rhs->v.primary);

	case RHS_INTERNAL :
	    return TYPE_FLOAT;	/* FIXME: actually, most internals are int */

	case RHS_OP :
	    if (rhs->v.op.op->type_prop == TYPE_PROP_CONST)
		return rhs->v.op.op->const_type;
	    else
	    {
		int max = TYPE_INT;
		int i;

		assert(rhs->v.op.op->type_prop == TYPE_PROP_MAX);

		for (i = 0; i < rhs->v.op.op->num_args; ++i)
		{
		    int type = primary_type(&rhs->v.op.args[i]);

		    if (type > max)
			max = type;
		}

		return max;
	    }
	    break;

	default :
	    assert(0);
    }

    return 0;
}

statement_list_t*
prepend_compvar_statements (compvar_t *compvar, statement_list_t *rest)
{
    value_t *value;

    for (value = compvar->values; value != 0; value = value->next)
    {
	statement_list_t *lst;

	for (lst = value->uses; lst != 0; lst = lst->next)
	    rest = prepend_statement(lst->stmt, rest);
    }

    return rest;
}

void
propagate_types_initially (statement_t *stmt, statement_list_t **worklist)
{
    int type, type2;

    while (stmt != 0)
    {
	switch (stmt->type)
	{
	    case STMT_NIL :
		break;

	    case STMT_ASSIGN :
		type = rhs_type(stmt->v.assign.rhs);
		if (type != stmt->v.assign.lhs->compvar->type)
		{
		    stmt->v.assign.lhs->compvar->type = type;
		    *worklist = prepend_compvar_statements(stmt->v.assign.lhs->compvar, *worklist);
		}
		break;

	    case STMT_PHI_ASSIGN :
		type = rhs_type(stmt->v.assign.rhs);
		type2 = rhs_type(stmt->v.assign.rhs2);
		if (type != type2)
		{
		    if (type2 > type)
			type = type2;
		}
		if (type != stmt->v.assign.lhs->compvar->type)
		{
		    stmt->v.assign.lhs->compvar->type = type;
		    *worklist = prepend_compvar_statements(stmt->v.assign.lhs->compvar, *worklist);
		}
		break;

	    case STMT_IF_COND :
		propagate_types_initially(stmt->v.if_cond.consequent, worklist);
		propagate_types_initially(stmt->v.if_cond.alternative, worklist);
		break;

	    case STMT_WHILE_LOOP :
		propagate_types_initially(stmt->v.while_loop.entry, worklist);
		propagate_types_initially(stmt->v.while_loop.body, worklist);
		break;

	    default :
		assert(0);
	}

	stmt = stmt->next;
    }
}

void
propagate_types_from_worklist (statement_list_t *worklist, statement_list_t **new_worklist)
{
    while (worklist != 0)
    {
	statement_t *stmt = worklist->stmt;
	int type, type2;

	assert(stmt->type == STMT_ASSIGN || stmt->type == STMT_PHI_ASSIGN);

	type = rhs_type(stmt->v.assign.rhs);
	if (stmt->type == STMT_PHI_ASSIGN)
	{
	    type2 = rhs_type(stmt->v.assign.rhs2);
	    if (type != type2)
	    {
		if (type2 > type)
		    type = type2;
	    }
	}

	if (type != stmt->v.assign.lhs->compvar->type)
	{
	    stmt->v.assign.lhs->compvar->type = type;
	    *new_worklist = prepend_compvar_statements(stmt->v.assign.lhs->compvar, *new_worklist);
	}

	worklist = worklist->next;
    }
}

void
propagate_types (void)
{
    statement_list_t *worklist = 0;

    propagate_types_initially(first_stmt, &worklist);
    do
    {
	statement_list_t *new_worklist = 0;

	propagate_types_from_worklist(worklist, &new_worklist);
	worklist = new_worklist;
    } while (worklist != 0);
}

void
output_compvar_name (FILE *out, compvar_t *compvar)
{
    if (compvar->var != 0)
	fprintf(out, "var_%s_%d", compvar->var->name, compvar->n);
    else
	fprintf(out, "tmp_%d", compvar->temp->number);
}

void
output_compvar_decl (FILE *out, compvar_t *compvar)
{
    if (!compvar->flag)
    {
	switch (compvar->type)
	{
	    case TYPE_INT :
		fputs("int ", out);
		break;

	    case TYPE_FLOAT :
		fputs("float ", out);
		break;

	    case TYPE_COLOR :
		fputs("color_t ", out);
		break;

	    case TYPE_COMPLEX :
		fputs("gsl_complex ", out);
		break;

	    case TYPE_MATRIX :
		fputs("gsl_matrix *", out);
		break;

	    case TYPE_VECTOR :
		fputs("gsl_vector *", out);
		break;
	}

	output_compvar_name(out, compvar);
	fputs(";\n", out);
	compvar->flag = 1;
    }
}

void
output_rhs_decl (FILE *out, rhs_t *rhs)
{
    if (rhs->type == RHS_PRIMARY && rhs->v.primary.type == PRIMARY_VALUE)
	output_compvar_decl(out, rhs->v.primary.v.value->compvar);
    else if (rhs->type == RHS_OP)
    {
	int i;

	for (i = 0; i < rhs->v.op.op->num_args; ++i)
	    if (rhs->v.op.args[i].type == PRIMARY_VALUE)
		output_compvar_decl(out, rhs->v.op.args[i].v.value->compvar);
    }
}

void
output_decls (FILE *out, statement_t *stmt)
{
    while (stmt != 0)
    {
	switch (stmt->type)
	{
	    case STMT_NIL :
		break;

	    case STMT_ASSIGN :
		output_compvar_decl(out, stmt->v.assign.lhs->compvar);
		output_rhs_decl(out, stmt->v.assign.rhs);
		break;

	    case STMT_PHI_ASSIGN :
		output_compvar_decl(out, stmt->v.assign.lhs->compvar);
		output_rhs_decl(out, stmt->v.assign.rhs);
		output_rhs_decl(out, stmt->v.assign.rhs2);
		break;

	    case STMT_IF_COND :
		output_rhs_decl(out, stmt->v.if_cond.condition);
		output_decls(out, stmt->v.if_cond.consequent);
		output_decls(out, stmt->v.if_cond.alternative);
		break;

	    case STMT_WHILE_LOOP :
		output_rhs_decl(out, stmt->v.while_loop.invariant);
		output_decls(out, stmt->v.while_loop.entry);
		output_decls(out, stmt->v.while_loop.body);
		break;

	    default :
		assert(0);
	}

	stmt = stmt->next;
    }
}

statement_t*
skip_phis (statement_t *stmt)
{
    while (stmt != 0 && stmt->type == STMT_PHI_ASSIGN)
	stmt = stmt->next;
    return stmt;
}

void
output_primary (FILE *out, primary_t *prim)
{
    switch (prim->type)
    {
	case PRIMARY_VALUE :
	    output_compvar_name(out, prim->v.value->compvar);
	    break;

	case PRIMARY_INT_CONST :
	    fprintf(out, "%d", prim->v.int_const);
	    break;

	case PRIMARY_FLOAT_CONST :
	    fprintf(out, "%f", prim->v.float_const);
	    break;
    }
}

void
output_rhs (FILE *out, rhs_t *rhs)
{
    switch (rhs->type)
    {
	case RHS_PRIMARY :
	    output_primary(out, &rhs->v.primary);
	    break;

	case RHS_INTERNAL :
	    fprintf(out, "invocation->internals[%d].data[0]", rhs->v.internal->index);
	    break;

	case RHS_OP :
	    {
		int i;

		fprintf(out, "%s(", rhs->v.op.op->name);
		for (i = 0; i < rhs->v.op.op->num_args; ++i)
		{
		    if (i > 0)
			fputs(",", out);
		    output_primary(out, &rhs->v.op.args[i]);
		}
		fputs(")", out);
	    }
	    break;

	default :
	    assert(0);
    }
}

void
output_phis (FILE *out, statement_t *phis, int branch)
{
    while (phis != 0 && phis->type == STMT_PHI_ASSIGN)
    {
	rhs_t *rhs = ((branch == 0) ? phis->v.assign.rhs : phis->v.assign.rhs2);

	if (rhs->type != RHS_PRIMARY
	    || rhs->v.primary.type != PRIMARY_VALUE
	    || rhs->v.primary.v.value->compvar != phis->v.assign.lhs->compvar)
	{
	    output_compvar_name(out, phis->v.assign.lhs->compvar);
	    fputs(" = ", out);
	    output_rhs(out, rhs);
	    fputs(";\n", out);
	}

	phis = phis->next;
    }
}

void
output_stmts (FILE *out, statement_t *stmt)
{
    while (stmt != 0)
    {
	switch (stmt->type)
	{
	    case STMT_NIL :
		stmt = stmt->next;
		break;

	    case STMT_ASSIGN :
		output_compvar_name(out, stmt->v.assign.lhs->compvar);
		fputs(" = ", out);
		output_rhs(out, stmt->v.assign.rhs);
		fputs(";\n", out);

		stmt = stmt->next;
		break;

	    case STMT_PHI_ASSIGN :
		assert(0);
		break;

	    case STMT_IF_COND :
		fputs("if (", out);
		output_rhs(out, stmt->v.if_cond.condition);
		fputs(")\n{\n", out);
		output_stmts(out, stmt->v.if_cond.consequent);
		output_phis(out, stmt->next, 0);
		fputs("}\nelse\n{\n", out);
		output_stmts(out, stmt->v.if_cond.alternative);
		output_phis(out, stmt->next, 1);
		fputs("}\n", out);

		stmt = skip_phis(stmt->next);
		break;

	    case STMT_WHILE_LOOP :
		output_phis(out, stmt->v.while_loop.entry, 0);
		fputs("while (", out);
		output_rhs(out, stmt->v.while_loop.invariant);
		fputs(")\n{\n", out);
		output_stmts(out, stmt->v.while_loop.body);
		output_phis(out, stmt->v.while_loop.entry, 1);
		fputs("}\n", out);

		stmt = stmt->next;
		break;

	    default :
		assert(0);
	}
    }
}

void
output_c_code (FILE *out)
{
    output_decls(out, first_stmt);
    output_stmts(out, first_stmt);
}

#ifdef OPENSTEP
#define	MAX(a,b)	(((a)<(b))?(b):(a))
#define	CGEN_CC		"cc -c -o" 
#define	CGEN_LD		"cc -bundle -o"
#endif

initfunc_t
gen_and_load_c_code (mathmap_t *mathmap, void **module_info)
{
    FILE *template = fopen("new_template.c", "r");
    FILE *out;
    int numtmpvars = 0, i;
    variable_t *var;
    char *buf;
    int pid = getpid();
    int c;
    initfunc_t initfunc;
    compvar_t *result[MAX_TUPLE_LENGTH];
#ifndef OPENSTEP
    GModule *module = 0;
#endif

    first_stmt = 0;
    emit_loc = &first_stmt;
    next_stmt_index = 0;
    next_temp_number = 1;

    assert(template != 0);

    gen_code(mathmap->exprtree, result, 0);
    propagate_types();

    buf = (char*)malloc(MAX(strlen(CGEN_CC), strlen(CGEN_LD)) + 512);

    sprintf(buf, "/tmp/mathfunc%d.c", pid);
    out = fopen(buf, "w");
    assert(out != 0);

    while ((c = fgetc(template)) != EOF)
    {
	if (c == '$')
	{
	    c = fgetc(template);
	    assert(c != EOF);

	    switch (c)
	    {
		case 'l' :
		    fprintf(out, "%d", MAX_TUPLE_LENGTH);
		    break;

		case 'g' :
#ifdef GIMP
		    putc('1', out);
#else
		    putc('0', out);
#endif
		    break;

		case 'm' :
		    {
			output_c_code(out);

			for (i = 0; i < mathmap->exprtree->result.length; ++i)
			{
			    fprintf(out, "invocation->stack[0].data[%d] = ", i);
			    output_compvar_name(out, result[i]);
			    fprintf(out, ";\n");
			}

			fprintf(out,
				"invocation->stack[0].length = %d;\n"
				"return &invocation->stack[0];\n",
				mathmap->exprtree->result.length);
		    }
		    break;

		case 'p' :
		    fprintf(out, "%d", USER_CURVE_POINTS);
		    break;

		case 'q' :
		    fprintf(out, "%d", USER_GRADIENT_POINTS);
		    break;

		case 'o' :
#ifdef OPENSTEP
		    putc('1', out);
#else
		    putc('0', out);
#endif
		    break;

		default :
		    putc(c, out);
		    break;
	    }
	}
	else
	    putc(c, out);
    }

    fclose(out);
    fclose(template);

#ifndef TEST
    sprintf(buf, "%s /tmp/mathfunc%d.o /tmp/mathfunc%d.c", CGEN_CC, pid, pid);
    system(buf);

    sprintf(buf, "%s /tmp/mathfunc%d.so /tmp/mathfunc%d.o", CGEN_LD, pid, pid);
    system(buf);

    sprintf(buf, "/tmp/mathfunc%d.so", pid);

#ifndef OPENSTEP
    module = g_module_open(buf, 0);
    if (module == 0)
    {
	fprintf(stderr, "could not load module: %s\n", g_module_error());
	assert(0);
    }

    printf("loaded %p\n", module);

    assert(g_module_symbol(module, "mathmapinit", (void**)&initfunc));

    unlink(buf);

    sprintf(buf, "/tmp/mathfunc%d.o", pid);
    unlink(buf);

    /*
    sprintf(buf, "/tmp/mathfunc%d.c", pid);
    unlink(buf);
    */

    *module_info = module;
#else
    {
        NSObjectFileImage objectFileImage;
        NSModule module;
        const char *moduleName = "Johnny";
        NSSymbol symbol;
        
        NSCreateObjectFileImageFromFile(
            buf, &objectFileImage);

        module = NSLinkModule(
            objectFileImage, moduleName,
            NSLINKMODULE_OPTION_PRIVATE | NSLINKMODULE_OPTION_BINDNOW);
        NSDestroyObjectFileImage(objectFileImage);

        {
            symbol = NSLookupSymbolInModule(
                module, "__init");
            if (symbol) {
                void (*init) (void) = NSAddressOfSymbol(symbol);
                init();
            }
        }

        symbol = NSLookupSymbolInModule(
            module, "_mathmapinit");
        initfunc = NSAddressOfSymbol(symbol);

	*module_info = module;
    }
#endif
#endif

    free(buf);

    return initfunc;
}

void
unload_c_code (void *module_info)
{
#ifndef OPENSTEP
    GModule *module = module_info;

    printf("unloading %p\n", module);
    assert(g_module_close(module));
#else
    /* FIXME */
#endif
}

void
test_compiler (void)
{
    variable_t *vars = 0;

    compvar_t *a = make_variable(register_variable(&vars, "a", make_tuple_info(nil_tag_number, 1)), 0);
    compvar_t *t = make_temporary();
    compvar_t *b = make_variable(register_variable(&vars, "b", make_tuple_info(nil_tag_number, 1)), 0);

    emit_assign(make_lhs(a), make_int_const_rhs(0));
    emit_assign(make_lhs(b), make_int_const_rhs(1));
    emit_assign(make_lhs(t), make_compvar_rhs(a));
    emit_assign(make_lhs(a), make_compvar_rhs(b));
    emit_assign(make_lhs(b), make_compvar_rhs(t));
    emit_assign(make_lhs(a), make_op_rhs(OP_ADD, make_compvar_primary(a), make_compvar_primary(b)));

    start_if_cond(make_compvar_rhs(a));
    /* emit_assign(make_lhs(b), make_int_const_rhs(1)); */
    switch_if_branch();
    emit_assign(make_lhs(b), make_int_const_rhs(2));
    end_if_cond();

    start_while_loop(make_compvar_rhs(a));
    emit_assign(make_lhs(b), make_compvar_rhs(b));
    emit_assign(make_lhs(b), make_int_const_rhs(1));
    emit_assign(make_lhs(b), make_int_const_rhs(3));
    end_while_loop();

    dump_code(first_stmt, 0);
}

void init_internals (mathmap_t *mathmap);

void
test_mathmap_compiler (char *expr)
{
    static mathmap_t mathmap;
    /* static compvar_t *result[MAX_TUPLE_LENGTH]; */

    mathmap.variables = 0;
    mathmap.userval_infos = 0;
    mathmap.internals = 0;

    mathmap.exprtree = 0;

    init_internals(&mathmap);

    the_mathmap = &mathmap;

    DO_JUMP_CODE {
	scanFromString(expr);
	yyparse();
	endScanningFromString();

	the_mathmap = 0;

	assert(mathmap.exprtree != 0);

	gen_and_load_c_code(&mathmap, 0);

	dump_code(first_stmt, 0);

	/*
	gen_code(mathmap.exprtree, result, 0);

	output_c_code();
	*/
    } WITH_JUMP_HANDLER {
	printf("%s\n", error_string);
	assert(0);
    } END_JUMP_HANDLER;
}

static void
init_op (int index, char *name, int num_args, type_prop_t type_prop, type_t const_type)
{
    assert(num_args <= MAX_OP_ARGS);

    ops[index].name = name;
    ops[index].num_args = num_args;
    ops[index].type_prop = type_prop;
    ops[index].const_type = const_type;
}

void
init_compiler (void)
{
    init_op(OP_NOP, "NOP", 0, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_ADD, "ADD", 2, TYPE_PROP_MAX, 0);
    init_op(OP_SUB, "SUB", 2, TYPE_PROP_MAX, 0);
    init_op(OP_NEG, "NEG", 1, TYPE_PROP_MAX, 0);
    init_op(OP_MUL, "MUL", 2, TYPE_PROP_MAX, 0);
    init_op(OP_DIV, "DIV", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_MOD, "MOD", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ABS, "abs", 1, TYPE_PROP_MAX, 0);
    init_op(OP_MIN, "MIN", 2, TYPE_PROP_MAX, 0);
    init_op(OP_MAX, "MAX", 2, TYPE_PROP_MAX, 0);
    init_op(OP_SQRT, "sqrt", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_HYPOT, "hypot", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_SIN, "sin", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_COS, "cos", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_TAN, "tan", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ASIN, "asin", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ACOS, "acos", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ATAN, "atan", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ATAN2, "atan2", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_POW, "pow", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_EXP, "exp", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_LOG, "log", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_SINH, "sinh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_COSH, "cosh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_TANH, "tanh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ASINH, "asinh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ACOSH, "acosh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ATANH, "atanh", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_GAMMA, "GAMMA", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_FLOOR, "floor", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_EQ, "EQ", 2, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_LESS, "LESS", 2, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_LEQ, "LEQ", 2, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_NOT, "NOT", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_PRINT, "PRINT", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_NEWLINE, "NEWLINE", 0, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_ORIG_VAL, "ORIG_VAL", 4, TYPE_PROP_CONST, TYPE_COLOR);
    init_op(OP_RED, "RED_FLOAT", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_GREEN, "GREEN_FLOAT", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_BLUE, "BLUE_FLOAT", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_ALPHA, "ALPHA_FLOAT", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_COMPLEX, "COMPLEX", 2, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_REAL, "C_REAL", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_C_IMAG, "C_IMAG", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_C_SQRT, "gsl_complex_sqrt", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_SIN, "gsl_complex_sin", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_COS, "gsl_complex_cos", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_TAN, "gsl_complex_tan", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ASIN, "gsl_complex_asin", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ACOS, "gsl_complex_acos", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ATAN, "gsl_complex_atan", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_POW, "gsl_complex_pow", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_EXP, "gsl_complex_exp", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_LOG, "gsl_complex_log", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ARG, "gsl_complex_arg", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_C_SINH, "gsl_complex_sinh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_COSH, "gsl_complex_cosh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_TANH, "gsl_complex_tanh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ASINH, "gsl_complex_asinh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ACOSH, "gsl_complex_acosh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_ATANH, "gsl_complex_atanh", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_C_GAMMA, "gsl_complex_gamma", 1, TYPE_PROP_CONST, TYPE_COMPLEX);
    init_op(OP_MAKE_M2X2, "MAKE_M2X2", 4, TYPE_PROP_CONST, TYPE_MATRIX);
    init_op(OP_MAKE_M3X3, "MAKE_M3X3", 9, TYPE_PROP_CONST, TYPE_MATRIX);
    init_op(OP_FREE_MATRIX, "FREE_MATRIX", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_MAKE_V2, "MAKE_V2", 2, TYPE_PROP_CONST, TYPE_VECTOR);
    init_op(OP_MAKE_V3, "MAKE_V3", 3, TYPE_PROP_CONST, TYPE_VECTOR);
    init_op(OP_FREE_VECTOR, "FREE_VECTOR", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_VECTOR_NTH, "VECTOR_NTH", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_SOLVE_LINEAR_2, "SOLVE_LINEAR_2", 2, TYPE_PROP_CONST, TYPE_VECTOR);
    init_op(OP_SOLVE_LINEAR_3, "SOLVE_LINEAR_3", 2, TYPE_PROP_CONST, TYPE_VECTOR);
    init_op(OP_NOISE, "noise", 3, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_RAND, "RAND", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_USERVAL_INT, "USERVAL_INT", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_USERVAL_FLOAT, "USERVAL_FLOAT", 1, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_USERVAL_BOOL, "USERVAL_BOOL", 1, TYPE_PROP_CONST, TYPE_INT);
    init_op(OP_USERVAL_CURVE, "USERVAL_CURVE", 2, TYPE_PROP_CONST, TYPE_FLOAT);
    init_op(OP_USERVAL_COLOR, "USERVAL_COLOR", 1, TYPE_PROP_CONST, TYPE_COLOR);
    init_op(OP_USERVAL_GRADIENT, "USERVAL_GRADIENT", 2, TYPE_PROP_CONST, TYPE_COLOR);
}

#ifdef TEST
int
main (int argc, char *argv[])
{
    assert(argc == 2);

    init_tags();
    init_builtins();
    init_macros();
    init_compiler();

    /* test_compiler(); */
    test_mathmap_compiler(argv[1]);
    return 0;
}
#endif