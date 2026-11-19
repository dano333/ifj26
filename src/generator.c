/*
 * Project: IFJ26
 * Authors: David Pavlištík (xpavlid00), Jakub Krčmárik (xkrcmaj00)
 * Description: generator of IFJcode26 (stack based code)
 */

#include "generator.h"
#include "error.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static FILE *gen_out = NULL;
static int label_counter = 0;
static ErrorCode gen_err = ERR_OK;

static void emit(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vfprintf(gen_out, fmt, ap);
	va_end(ap);
	fputc('\n', gen_out);
}

/* string constant: characters 000-032, 035, 092 (and non ASCII) are written as \xyz (spec 7.3) */
static void emit_string_const(const char *s) {
	fputs("string@", gen_out);
	for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
		if (*p <= 32 || *p == 35 || *p == 92 || *p >= 127) {
			fprintf(gen_out, "\\%03d", *p);
		} else {
			fputc(*p, gen_out);
		}
	}
}

static int new_label(void) { return label_counter++; }

/* owned strings (unique variable names); freed at the end of the program */
static char **pool = NULL;
static int pool_count = 0;
static int pool_cap = 0;
static int var_counter = 0;

static char *pool_add(char *s) {
	if (s == NULL) {
		gen_err = ERR_INTERNAL_COMPILER;
		return NULL;
	}
	if (pool_count == pool_cap) {
		int ncap = pool_cap == 0 ? 64 : pool_cap * 2;
		char **tmp = realloc(pool, (size_t)ncap * sizeof(char *));
		if (tmp == NULL) {
			free(s);
			gen_err = ERR_INTERNAL_COMPILER;
			return NULL;
		}
		pool = tmp;
		pool_cap = ncap;
	}
	pool[pool_count++] = s;
	return s;
}

static char *pool_printf(const char *fmt, ...) {
	char buf[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	size_t n = strlen(buf) + 1;
	char *s = malloc(n);
	if (s != NULL) {
		memcpy(s, buf, n);
	}
	return pool_add(s);
}

/* user functions and extension methods of the program (needed for argument types and labels) */
typedef struct {
	const AstNode *fn;
	DataType recv_type; /* TYPE_UNKNOWN for plain functions */
} FuncInfo;

static FuncInfo *funcs = NULL;
static int func_count = 0;

static void func_add(const AstNode *fn, DataType recv_type) {
	FuncInfo *tmp = realloc(funcs, (size_t)(func_count + 1) * sizeof(FuncInfo));
	if (tmp == NULL) {
		gen_err = ERR_INTERNAL_COMPILER;
		return;
	}
	funcs = tmp;
	funcs[func_count].fn = fn;
	funcs[func_count].recv_type = recv_type;
	func_count++;
}

static const AstNode *find_function(const char *name) {
	for (int i = 0; i < func_count; i++) {
		if (funcs[i].recv_type == TYPE_UNKNOWN && strcmp(funcs[i].fn->ast.func_node.name, name) == 0) {
			return funcs[i].fn;
		}
	}
	return NULL;
}

static const AstNode *find_method(DataType recv, const char *name) {
	for (int i = 0; i < func_count; i++) {
		if (funcs[i].recv_type == recv && funcs[i].recv_type != TYPE_UNKNOWN &&
		    strcmp(funcs[i].fn->ast.func_node.name, name) == 0) {
			return funcs[i].fn;
		}
	}
	return NULL;
}

static const char *type_label_name(DataType t) {
	switch (t) {
	case TYPE_INT:
		return "Int";
	case TYPE_DOUBLE:
		return "Double";
	case TYPE_STRING:
		return "String";
	case TYPE_BOOLEAN:
		return "Boolean";
	case TYPE_UNIT:
		return "Unit";
	default:
		return "Any";
	}
}

/* declarations of one function: node (+slot) -> unique variable name */
typedef struct {
	const AstNode *node;
	int slot; /* 0 = the variable itself, 1 = loop limit, 2 = loop step */
	char *uname;
} Decl;

static Decl *decls = NULL;
static int decl_count = 0;
static int decl_cap = 0;

static void decl_add(const AstNode *node, int slot, const char *name) {
	if (decl_count == decl_cap) {
		int ncap = decl_cap == 0 ? 16 : decl_cap * 2;
		Decl *tmp = realloc(decls, (size_t)ncap * sizeof(Decl));
		if (tmp == NULL) {
			gen_err = ERR_INTERNAL_COMPILER;
			return;
		}
		decls = tmp;
		decl_cap = ncap;
	}
	char *u = pool_printf("%s%%%d", name, var_counter++);
	if (u == NULL) {
		return;
	}
	decls[decl_count].node = node;
	decls[decl_count].slot = slot;
	decls[decl_count].uname = u;
	decl_count++;
}

static const char *decl_name(const AstNode *node, int slot) {
	for (int i = 0; i < decl_count; i++) {
		if (decls[i].node == node && decls[i].slot == slot) {
			return decls[i].uname;
		}
	}
	gen_err = ERR_INTERNAL_COMPILER;
	return "%missing";
}

/* visible variables (scope stack) */
typedef struct {
	const char *name;
	const char *uname;
	DataType type;
} Var;

static Var *vars = NULL;
static int var_count = 0;
static int var_cap = 0;
static int *marks = NULL;
static int mark_count = 0;
static int mark_cap = 0;

static void scope_push(void) {
	if (mark_count == mark_cap) {
		int ncap = mark_cap == 0 ? 16 : mark_cap * 2;
		int *tmp = realloc(marks, (size_t)ncap * sizeof(int));
		if (tmp == NULL) {
			gen_err = ERR_INTERNAL_COMPILER;
			return;
		}
		marks = tmp;
		mark_cap = ncap;
	}
	marks[mark_count++] = var_count;
}

static void scope_pop(void) {
	if (mark_count > 0) {
		var_count = marks[--mark_count];
	}
}

static void var_add(const char *name, const char *uname, DataType type) {
	if (var_count == var_cap) {
		int ncap = var_cap == 0 ? 32 : var_cap * 2;
		Var *tmp = realloc(vars, (size_t)ncap * sizeof(Var));
		if (tmp == NULL) {
			gen_err = ERR_INTERNAL_COMPILER;
			return;
		}
		vars = tmp;
		var_cap = ncap;
	}
	vars[var_count].name = name;
	vars[var_count].uname = uname;
	vars[var_count].type = type;
	var_count++;
}

static const Var *var_find(const char *name) {
	for (int i = var_count - 1; i >= 0; i--) {
		if (strcmp(vars[i].name, name) == 0) {
			return &vars[i];
		}
	}
	gen_err = ERR_INTERNAL_COMPILER;
	return NULL;
}

static void collect(const AstNode *n) {
	if (n == NULL) {
		return;
	}
	switch (n->kind) {
	case NODE_BLOCK:
		for (int i = 0; i < n->ast.block_node.block.count; i++) {
			collect(n->ast.block_node.block.statements[i]);
		}
		break;
	case NODE_VAL_DEF:
	case NODE_VAR_DEF:
		collect(n->ast.var_val.initializer);
		decl_add(n, 0, n->ast.var_val.name);
		break;
	case NODE_ASSIGN:
		collect(n->ast.assign.value);
		break;
	case NODE_EXPR_STMT:
	case NODE_RETURN:
		collect(n->ast.expr_stmt.child);
		break;
	case NODE_IF_STMT:
	case NODE_IF_EXPR:
		collect(n->ast.if_node.cond);
		collect(n->ast.if_node.then_branch);
		collect(n->ast.if_node.else_branch);
		break;
	case NODE_WHILE:
		collect(n->ast.while_node.cond);
		collect(n->ast.while_node.body);
		break;
	case NODE_FOR:
		collect(n->ast.for_node.from);
		collect(n->ast.for_node.to);
		collect(n->ast.for_node.by);
		for (int i = 0; i < n->ast.for_node.guard_count; i++) {
			collect(n->ast.for_node.guards[i]);
		}
		collect(n->ast.for_node.body);
		decl_add(n, 0, n->ast.for_node.var_name);
		decl_add(n, 1, "_limit");
		decl_add(n, 2, "_step");
		break;
	case NODE_UNARY_OP:
		collect(n->ast.unary.operand);
		break;
	case NODE_BINARY_OP:
		collect(n->ast.binary.left);
		collect(n->ast.binary.right);
		break;
	case NODE_CALL:
		for (int i = 0; i < n->ast.call_node.arg_count; i++) {
			collect(n->ast.call_node.args[i]);
		}
		break;
	case NODE_MEMBER:
		collect(n->ast.member_node.receiver);
		for (int i = 0; i < n->ast.member_node.arg_count; i++) {
			collect(n->ast.member_node.args[i]);
		}
		break;
	case NODE_ISTRING:
		for (int i = 0; i < n->ast.istring_node.part_count; i++) {
			collect(n->ast.istring_node.parts[i]);
		}
		break;
	default:
		break;
	}
}

static void gen_expr(const AstNode *n);
static void gen_stmt(const AstNode *n);

static DataType cur_ret = TYPE_UNIT; /* return type of the function being generated */

/* Int is the only type that is implicitly converted (to Double) */
static void gen_coerce(DataType from, DataType to) {
	if (from == TYPE_INT && to == TYPE_DOUBLE) {
		emit("INT2FLOATS");
	}
}

static void gen_expr_to(const AstNode *n, DataType to) {
	gen_expr(n);
	gen_coerce(n->type, to);
}

static bool is_numeric(DataType t) { return t == TYPE_INT || t == TYPE_DOUBLE; }

/* jumps to `label` when the Boolean on top of the stack is false (the value is consumed) */
static void gen_jump_if_false(const char *fmt, int id) {
	emit("PUSHS bool@false");
	fputs("JUMPIFEQS ", gen_out);
	fprintf(gen_out, fmt, id);
	fputc('\n', gen_out);
}

/* the value of a block or of a single expression (branches of an if expression, function bodies) */
static void gen_value(const AstNode *n, DataType to) {
	if (n == NULL) {
		emit("PUSHS nil@nil");
		return;
	}
	if (n->kind != NODE_BLOCK) {
		gen_expr_to(n, to);
		return;
	}
	scope_push();
	int cnt = n->ast.block_node.block.count;
	for (int i = 0; i < cnt - 1; i++) {
		gen_stmt(n->ast.block_node.block.statements[i]);
	}
	const AstNode *last = cnt > 0 ? n->ast.block_node.block.statements[cnt - 1] : NULL;
	if (last != NULL && last->kind == NODE_EXPR_STMT && last->ast.expr_stmt.child != NULL) {
		gen_expr_to(last->ast.expr_stmt.child, to);
	} else {
		if (last != NULL) {
			gen_stmt(last);
		}
		emit("PUSHS nil@nil");
	}
	scope_pop();
}

/* types of the parameters of the built-in functions (TYPE_UNKNOWN = Any) */
static const DataType *builtin_ptypes(const char *name, int *count) {
	static const DataType any[] = {TYPE_UNKNOWN};
	static const DataType str1[] = {TYPE_STRING};
	static const DataType str_int[] = {TYPE_STRING, TYPE_INT};
	static const DataType int1[] = {TYPE_INT};
	static const DataType sub[] = {TYPE_STRING, TYPE_INT, TYPE_INT};
	static const DataType dbl[] = {TYPE_DOUBLE};
	*count = 1;
	if (strcmp(name, "print") == 0 || strcmp(name, "toStr") == 0) {
		return any;
	}
	if (strcmp(name, "strLength") == 0) {
		return str1;
	}
	if (strcmp(name, "ord") == 0) {
		*count = 2;
		return str_int;
	}
	if (strcmp(name, "chr") == 0) {
		return int1;
	}
	if (strcmp(name, "subStr") == 0) {
		*count = 3;
		return sub;
	}
	if (strcmp(name, "toInt") == 0 || strcmp(name, "floor") == 0) {
		return dbl;
	}
	*count = 0;
	return NULL;
}

/*
 * Generic call: [receiver] and args are evaluated onto the data stack (converted to the parameter types),
 * then the frame of the callee is prepared and the call is made. `ptypes` has one entry per argument
 * (not per receiver), NULL means "keep the types", TYPE_UNKNOWN means Any.
 */
static void gen_invoke(const char *label, const AstNode *recv, DataType recv_to, AstNode *const *args, int n,
                       const DataType *ptypes) {
	int total = n + (recv != NULL ? 1 : 0);
	if (recv != NULL) {
		gen_expr_to(recv, recv_to);
	}
	for (int i = 0; i < n; i++) {
		DataType to = (ptypes != NULL && ptypes[i] != TYPE_UNKNOWN) ? ptypes[i] : args[i]->type;
		gen_expr_to(args[i], to);
	}
	emit("CREATEFRAME");
	for (int i = 0; i < total; i++) {
		emit("DEFVAR TF@%%p%d", i);
	}
	for (int i = total - 1; i >= 0; i--) {
		emit("POPS TF@%%p%d", i);
	}
	emit("CALL %s", label);
}

static void param_types_of(const AstNode *fn, DataType *out, int n) {
	for (int i = 0; i < n; i++) {
		out[i] = fn->ast.func_node.params[i]->ast.param_node.type;
	}
}

static void gen_call(const AstNode *n) {
	const char *name = n->ast.call_node.name;
	int argc = n->ast.call_node.arg_count;
	const AstNode *fn = find_function(name);

	if (fn != NULL) {
		DataType *pt = NULL;
		if (argc > 0) {
			pt = malloc((size_t)argc * sizeof(DataType));
			if (pt == NULL) {
				gen_err = ERR_INTERNAL_COMPILER;
				return;
			}
			param_types_of(fn, pt, argc < fn->ast.func_node.param_count ? argc : fn->ast.func_node.param_count);
		}
		char label[300];
		snprintf(label, sizeof label, "$fn_%s", name);
		gen_invoke(label, NULL, TYPE_UNKNOWN, n->ast.call_node.args, argc, pt);
		free(pt);
		return;
	}

	int pc = 0;
	const DataType *pt = builtin_ptypes(name, &pc);
	char label[300];
	snprintf(label, sizeof label, "$ifj_%s", name);
	gen_invoke(label, NULL, TYPE_UNKNOWN, n->ast.call_node.args, argc, pt);
}

/* x.name / x.name(args): a user extension method or one of the built-in members (VALMEM) */
static void gen_member(const AstNode *n) {
	const char *name = n->ast.member_node.member_name;
	const AstNode *recv = n->ast.member_node.receiver;
	int argc = n->ast.member_node.arg_count;
	const AstNode *m = find_method(recv->type, name);
	char label[300];

	if (m != NULL) {
		DataType *pt = NULL;
		if (argc > 0) {
			pt = malloc((size_t)argc * sizeof(DataType));
			if (pt == NULL) {
				gen_err = ERR_INTERNAL_COMPILER;
				return;
			}
			param_types_of(m, pt, argc < m->ast.func_node.param_count ? argc : m->ast.func_node.param_count);
		}
		snprintf(label, sizeof label, "$ext_%s_%s", type_label_name(recv->type), name);
		gen_invoke(label, recv, recv->type, n->ast.member_node.args, argc, pt);
		free(pt);
		return;
	}

	if (strcmp(name, "toString") == 0) {
		gen_invoke("$ifj_toStr", recv, recv->type, NULL, 0, NULL);
	} else if (strcmp(name, "length") == 0) {
		gen_invoke("$ifj_strLength", recv, TYPE_STRING, NULL, 0, NULL);
	} else if (strcmp(name, "toInt") == 0) {
		gen_invoke("$ifj_toInt", recv, TYPE_DOUBLE, NULL, 0, NULL);
	} else if (strcmp(name, "floor") == 0) {
		gen_invoke("$ifj_floor", recv, TYPE_DOUBLE, NULL, 0, NULL);
	} else if (find_function(name) != NULL) { /* f(receiver, args) */
		const AstNode *fn = find_function(name);
		DataType *pt = malloc((size_t)(argc + 1) * sizeof(DataType));
		if (pt == NULL) {
			gen_err = ERR_INTERNAL_COMPILER;
			return;
		}
		param_types_of(fn, pt, argc + 1 < fn->ast.func_node.param_count ? argc + 1 : fn->ast.func_node.param_count);
		snprintf(label, sizeof label, "$fn_%s", name);
		gen_invoke(label, recv, pt[0], n->ast.member_node.args, argc, pt + 1);
		free(pt);
	} else {
		gen_err = ERR_INTERNAL_COMPILER;
	}
}

static void gen_binary(const AstNode *n) {
	OpType op = n->ast.binary.op;
	const AstNode *l = n->ast.binary.left;
	const AstNode *r = n->ast.binary.right;

	/* short-circuit evaluation (BOOLTHEN) */
	if (op == OP_AND || op == OP_OR) {
		int id = new_label();
		gen_expr(l);
		emit("PUSHS bool@%s", op == OP_AND ? "false" : "true");
		emit("JUMPIFEQS $L%d_short", id);
		gen_expr(r);
		emit("JUMP $L%d_end", id);
		emit("LABEL $L%d_short", id);
		emit("PUSHS bool@%s", op == OP_AND ? "false" : "true");
		emit("LABEL $L%d_end", id);
		return;
	}

	/* a mixed Int/Double pair is computed as Double */
	DataType common = l->type;
	if (is_numeric(l->type) && is_numeric(r->type) && (l->type == TYPE_DOUBLE || r->type == TYPE_DOUBLE)) {
		common = TYPE_DOUBLE;
	}
	gen_expr_to(l, common);
	gen_expr_to(r, common);

	switch (op) {
	case OP_ADD:
		if (common == TYPE_STRING) {
			emit("POPS GF@_b");
			emit("POPS GF@_a");
			emit("CONCAT GF@_r GF@_a GF@_b");
			emit("PUSHS GF@_r");
		} else {
			emit("ADDS");
		}
		break;
	case OP_SUB:
		emit("SUBS");
		break;
	case OP_MUL:
		emit("MULS");
		break;
	case OP_DIV:
		emit(common == TYPE_DOUBLE ? "DIVS" : "IDIVS");
		break;
	case OP_LT:
		emit("LTS");
		break;
	case OP_GT:
		emit("GTS");
		break;
	case OP_LE: /* a <= b  ==  !(a > b) */
		emit("GTS");
		emit("NOTS");
		break;
	case OP_GE:
		emit("LTS");
		emit("NOTS");
		break;
	case OP_EQ:
		emit("EQS");
		break;
	case OP_NE:
		emit("EQS");
		emit("NOTS");
		break;
	default:
		gen_err = ERR_INTERNAL_COMPILER;
		break;
	}
}

static void gen_istring(const AstNode *n) {
	int cnt = n->ast.istring_node.part_count;
	emit("PUSHS string@");
	for (int i = 0; i < cnt; i++) {
		const AstNode *p = n->ast.istring_node.parts[i];
		gen_expr(p);
		if (p->type != TYPE_STRING) {
			emit("CREATEFRAME");
			emit("DEFVAR TF@%%p0");
			emit("POPS TF@%%p0");
			emit("CALL $ifj_toStr");
		}
		emit("POPS GF@_b");
		emit("POPS GF@_a");
		emit("CONCAT GF@_r GF@_a GF@_b");
		emit("PUSHS GF@_r");
	}
}

static void gen_expr(const AstNode *n) {
	if (n == NULL || gen_err != ERR_OK) {
		return;
	}
	switch (n->kind) {
	case NODE_LIT_INT:
		emit("PUSHS int@%lld", n->ast.literal.int_val);
		break;
	case NODE_LIT_DOUBLE:
		emit("PUSHS float@%a", n->ast.literal.double_val);
		break;
	case NODE_LIT_STRING:
		fputs("PUSHS ", gen_out);
		emit_string_const(n->ast.literal.str_val != NULL ? n->ast.literal.str_val : "");
		fputc('\n', gen_out);
		break;
	case NODE_LIT_BOOL:
		emit("PUSHS bool@%s", n->ast.literal.bool_val ? "true" : "false");
		break;
	case NODE_LIT_UNIT:
		emit("PUSHS nil@nil");
		break;
	case NODE_ID: {
		const Var *v = var_find(n->ast.identifier.name);
		if (v != NULL) {
			emit("PUSHS LF@%s", v->uname);
		}
		break;
	}
	case NODE_UNARY_OP:
		gen_expr(n->ast.unary.operand);
		if (n->ast.unary.op == OP_NEG) {
			if (n->ast.unary.operand->type == TYPE_DOUBLE) {
				emit("PUSHS float@%a", -1.0);
			} else {
				emit("PUSHS int@-1");
			}
			emit("MULS");
		} else if (n->ast.unary.op == OP_NOT) {
			emit("NOTS");
		}
		break;
	case NODE_BINARY_OP:
		gen_binary(n);
		break;
	case NODE_CALL:
		gen_call(n);
		break;
	case NODE_MEMBER:
		gen_member(n);
		break;
	case NODE_ISTRING:
		gen_istring(n);
		break;
	case NODE_IF_EXPR: {
		int id = new_label();
		gen_expr(n->ast.if_node.cond);
		gen_jump_if_false("$L%d_else", id);
		gen_value(n->ast.if_node.then_branch, n->type);
		emit("JUMP $L%d_end", id);
		emit("LABEL $L%d_else", id);
		gen_value(n->ast.if_node.else_branch, n->type);
		emit("LABEL $L%d_end", id);
		break;
	}
	default:
		gen_err = ERR_INTERNAL_COMPILER;
		break;
	}
}

static void gen_for(const AstNode *n) {
	int id = new_label();
	const char *var = decl_name(n, 0);
	const char *limit = decl_name(n, 1);
	const char *step = decl_name(n, 2);
	bool until = n->ast.for_node.is_until;
	bool has_step = n->ast.for_node.by != NULL;

	/* bounds and step are evaluated exactly once, before the first iteration */
	gen_expr(n->ast.for_node.from);
	emit("POPS LF@%s", var);
	gen_expr(n->ast.for_node.to);
	emit("POPS LF@%s", limit);
	if (has_step) {
		gen_expr(n->ast.for_node.by);
		emit("POPS LF@%s", step);
		emit("JUMPIFEQ $L%d_zero LF@%s int@0", id, step); /* zero step: runtime error 40 */
	} else {
		emit("MOVE LF@%s int@1", step);
	}

	emit("LABEL $L%d_test", id);
	if (has_step) {
		emit("PUSHS LF@%s", step);
		emit("PUSHS int@0");
		emit("GTS");
		emit("PUSHS bool@true");
		emit("JUMPIFEQS $L%d_up", id);
		/* negative step: i >= limit (to) or i > limit (until) */
		emit("PUSHS LF@%s", var);
		emit("PUSHS LF@%s", limit);
		if (until) {
			emit("GTS");
		} else {
			emit("LTS");
			emit("NOTS");
		}
		emit("JUMP $L%d_cond", id);
		emit("LABEL $L%d_up", id);
	}
	/* positive step: i <= limit (to) or i < limit (until) */
	emit("PUSHS LF@%s", var);
	emit("PUSHS LF@%s", limit);
	if (until) {
		emit("LTS");
	} else {
		emit("GTS");
		emit("NOTS");
	}
	emit("LABEL $L%d_cond", id);
	gen_jump_if_false("$L%d_end", id);

	scope_push();
	var_add(n->ast.for_node.var_name, var, TYPE_INT);
	for (int g = 0; g < n->ast.for_node.guard_count; g++) {
		gen_expr(n->ast.for_node.guards[g]);
		gen_jump_if_false("$L%d_next", id);
	}
	gen_stmt(n->ast.for_node.body);
	scope_pop();

	emit("LABEL $L%d_next", id);
	emit("ADD LF@%s LF@%s LF@%s", var, var, step);
	emit("JUMP $L%d_test", id);
	if (has_step) {
		emit("LABEL $L%d_zero", id);
		emit("EXIT int@40");
	}
	emit("LABEL $L%d_end", id);
}

static void gen_stmt(const AstNode *n) {
	if (n == NULL || gen_err != ERR_OK) {
		return;
	}
	switch (n->kind) {
	case NODE_BLOCK:
		scope_push();
		for (int i = 0; i < n->ast.block_node.block.count; i++) {
			gen_stmt(n->ast.block_node.block.statements[i]);
		}
		scope_pop();
		break;

	case NODE_VAL_DEF:
	case NODE_VAR_DEF: {
		const char *u = decl_name(n, 0);
		if (n->ast.var_val.initializer != NULL) {
			gen_expr_to(n->ast.var_val.initializer, n->type);
			emit("POPS LF@%s", u);
		}
		var_add(n->ast.var_val.name, u, n->type);
		break;
	}

	case NODE_ASSIGN: {
		const Var *v = var_find(n->ast.assign.name);
		if (v != NULL) {
			gen_expr_to(n->ast.assign.value, v->type);
			emit("POPS LF@%s", v->uname);
		}
		break;
	}

	case NODE_EXPR_STMT:
		if (n->ast.expr_stmt.child != NULL) {
			gen_expr(n->ast.expr_stmt.child);
			emit("POPS GF@_discard");
		}
		break;

	case NODE_RETURN:
		if (cur_ret == TYPE_UNIT || cur_ret == TYPE_UNKNOWN) {
			/* a Unit function discards the value of its return (spec 2.3, rule 4) */
			if (n->ast.expr_stmt.child != NULL) {
				gen_expr(n->ast.expr_stmt.child);
				emit("POPS GF@_discard");
			}
			emit("PUSHS nil@nil");
		} else {
			gen_expr_to(n->ast.expr_stmt.child, cur_ret);
		}
		emit("POPFRAME");
		emit("RETURN");
		break;

	case NODE_IF_STMT: {
		int id = new_label();
		gen_expr(n->ast.if_node.cond);
		gen_jump_if_false("$L%d_else", id);
		gen_stmt(n->ast.if_node.then_branch);
		emit("JUMP $L%d_end", id);
		emit("LABEL $L%d_else", id);
		gen_stmt(n->ast.if_node.else_branch);
		emit("LABEL $L%d_end", id);
		break;
	}

	case NODE_WHILE: {
		int id = new_label();
		emit("LABEL $L%d_start", id);
		gen_expr(n->ast.while_node.cond);
		gen_jump_if_false("$L%d_end", id);
		gen_stmt(n->ast.while_node.body);
		emit("JUMP $L%d_start", id);
		emit("LABEL $L%d_end", id);
		break;
	}

	case NODE_FOR:
		gen_for(n);
		break;

	default: /* an expression used as a statement */
		gen_expr(n);
		emit("POPS GF@_discard");
		break;
	}
}

static void gen_function(const AstNode *fn, const char *label, const char *recv_name, DataType recv_type) {
	const AstNode *body = fn->ast.func_node.body;
	int off = recv_name != NULL ? 1 : 0;

	decl_count = 0;
	var_count = 0;
	mark_count = 0;
	cur_ret = fn->ast.func_node.return_type;

	scope_push();
	if (recv_name != NULL) {
		var_add(recv_name, "%p0", recv_type);
	}
	for (int i = 0; i < fn->ast.func_node.param_count; i++) {
		const AstNode *p = fn->ast.func_node.params[i];
		var_add(p->ast.param_node.name, pool_printf("%%p%d", i + off), p->ast.param_node.type);
	}
	collect(body);

	emit("LABEL %s", label);
	emit("PUSHFRAME");
	for (int i = 0; i < decl_count; i++) {
		emit("DEFVAR LF@%s", decls[i].uname);
	}
	if (body != NULL && body->kind == NODE_BLOCK) {
		for (int i = 0; i < body->ast.block_node.block.count; i++) {
			gen_stmt(body->ast.block_node.block.statements[i]);
		}
	} else {
		gen_stmt(body);
	}
	/* reached only by Unit functions */
	emit("PUSHS nil@nil");
	emit("POPFRAME");
	emit("RETURN");
	scope_pop();
}

static const char *RUNTIME =
    /* print(value: Any): Unit - nil (the Unit value) is written as "()" */
    "LABEL $ifj_print\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%t\n"
    "TYPE LF@%t LF@%p0\n"
    "JUMPIFNEQ $ifj_print_w LF@%t string@nil\n"
    "WRITE string@()\n"
    "JUMP $ifj_print_e\n"
    "LABEL $ifj_print_w\n"
    "WRITE LF@%p0\n"
    "LABEL $ifj_print_e\n"
    "PUSHS nil@nil\n"
    "POPFRAME\n"
    "RETURN\n"
    /* toStr(value: Any): String */
    "LABEL $ifj_toStr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%t\n"
    "DEFVAR LF@%r\n"
    "TYPE LF@%t LF@%p0\n"
    "JUMPIFEQ $ifj_toStr_int LF@%t string@int\n"
    "JUMPIFEQ $ifj_toStr_float LF@%t string@float\n"
    "JUMPIFEQ $ifj_toStr_bool LF@%t string@bool\n"
    "JUMPIFEQ $ifj_toStr_str LF@%t string@string\n"
    "MOVE LF@%r string@()\n"
    "JUMP $ifj_toStr_end\n"
    "LABEL $ifj_toStr_int\n"
    "INT2STR LF@%r LF@%p0\n"
    "JUMP $ifj_toStr_end\n"
    "LABEL $ifj_toStr_float\n"
    "FLOAT2STR LF@%r LF@%p0\n"
    "JUMP $ifj_toStr_end\n"
    "LABEL $ifj_toStr_bool\n"
    "MOVE LF@%r string@false\n"
    "JUMPIFNEQ $ifj_toStr_end LF@%p0 bool@true\n"
    "MOVE LF@%r string@true\n"
    "JUMP $ifj_toStr_end\n"
    "LABEL $ifj_toStr_str\n"
    "MOVE LF@%r LF@%p0\n"
    "LABEL $ifj_toStr_end\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* readLine(): String - EOF gives "" */
    "LABEL $ifj_readLine\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "READ LF@%r string\n"
    "JUMPIFNEQ $ifj_readLine_e LF@%r nil@nil\n"
    "MOVE LF@%r string@\n"
    "LABEL $ifj_readLine_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* readInt(): Int - EOF or a wrong format gives 0 */
    "LABEL $ifj_readInt\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "READ LF@%r int\n"
    "JUMPIFNEQ $ifj_readInt_e LF@%r nil@nil\n"
    "MOVE LF@%r int@0\n"
    "LABEL $ifj_readInt_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* readDouble(): Double */
    "LABEL $ifj_readDouble\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "READ LF@%r float\n"
    "JUMPIFNEQ $ifj_readDouble_e LF@%r nil@nil\n"
    "MOVE LF@%r float@0x0p+0\n"
    "LABEL $ifj_readDouble_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* readBoolean(): Boolean - EOF or invalid input falls back to false */
    "LABEL $ifj_readBoolean\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "READ LF@%r bool\n"
    "JUMPIFNEQ $ifj_readBoolean_e LF@%r nil@nil\n"
    "MOVE LF@%r bool@false\n"
    "LABEL $ifj_readBoolean_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* strLength(value: String): Int */
    "LABEL $ifj_strLength\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "STRLEN LF@%r LF@%p0\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* ord(value: String, index: Int): Int - empty string or a wrong index gives -1 */
    "LABEL $ifj_ord\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "DEFVAR LF@%n\n"
    "DEFVAR LF@%c\n"
    "STRLEN LF@%n LF@%p0\n"
    "MOVE LF@%r int@-1\n"
    "LT LF@%c LF@%p1 int@0\n"
    "JUMPIFEQ $ifj_ord_e LF@%c bool@true\n"
    "LT LF@%c LF@%p1 LF@%n\n"
    "JUMPIFNEQ $ifj_ord_e LF@%c bool@true\n"
    "STRI2INT LF@%r LF@%p0 LF@%p1\n"
    "LABEL $ifj_ord_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* chr(code: Int): String */
    "LABEL $ifj_chr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "INT2CHAR LF@%r LF@%p0\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* subStr(value, start, end): String - an invalid range ends the program with 40 */
    "LABEL $ifj_subStr\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "DEFVAR LF@%n\n"
    "DEFVAR LF@%c\n"
    "DEFVAR LF@%i\n"
    "DEFVAR LF@%ch\n"
    "STRLEN LF@%n LF@%p0\n"
    "LT LF@%c LF@%p1 int@0\n"
    "JUMPIFEQ $ifj_subStr_err LF@%c bool@true\n"
    "LT LF@%c LF@%p2 int@0\n"
    "JUMPIFEQ $ifj_subStr_err LF@%c bool@true\n"
    "GT LF@%c LF@%p1 LF@%p2\n"
    "JUMPIFEQ $ifj_subStr_err LF@%c bool@true\n"
    "LT LF@%c LF@%p1 LF@%n\n"
    "JUMPIFNEQ $ifj_subStr_err LF@%c bool@true\n"
    "GT LF@%c LF@%p2 LF@%n\n"
    "JUMPIFEQ $ifj_subStr_err LF@%c bool@true\n"
    "MOVE LF@%r string@\n"
    "MOVE LF@%i LF@%p1\n"
    "LABEL $ifj_subStr_loop\n"
    "LT LF@%c LF@%i LF@%p2\n"
    "JUMPIFNEQ $ifj_subStr_e LF@%c bool@true\n"
    "GETCHAR LF@%ch LF@%p0 LF@%i\n"
    "CONCAT LF@%r LF@%r LF@%ch\n"
    "ADD LF@%i LF@%i int@1\n"
    "JUMP $ifj_subStr_loop\n"
    "LABEL $ifj_subStr_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    "LABEL $ifj_subStr_err\n"
    "EXIT int@40\n"
    /* toInt(value: Double): Int - truncation */
    "LABEL $ifj_toInt\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%r\n"
    "FLOAT2INT LF@%r LF@%p0\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n"
    /* floor(Double): Double (member of the VALMEM extension) */
    "LABEL $ifj_floor\n"
    "PUSHFRAME\n"
    "DEFVAR LF@%i\n"
    "DEFVAR LF@%r\n"
    "DEFVAR LF@%c\n"
    "FLOAT2INT LF@%i LF@%p0\n"
    "INT2FLOAT LF@%r LF@%i\n"
    "GT LF@%c LF@%r LF@%p0\n"
    "JUMPIFNEQ $ifj_floor_e LF@%c bool@true\n"
    "SUB LF@%r LF@%r float@0x1p+0\n"
    "LABEL $ifj_floor_e\n"
    "PUSHS LF@%r\n"
    "POPFRAME\n"
    "RETURN\n";

void generator_init(FILE *out) {
	gen_out = out;
	label_counter = 0;
	var_counter = 0;
	gen_err = ERR_OK;
}

static void generator_cleanup(void) {
	for (int i = 0; i < pool_count; i++) {
		free(pool[i]);
	}
	free(pool);
	free(funcs);
	free(decls);
	free(vars);
	free(marks);
	pool = NULL;
	funcs = NULL;
	decls = NULL;
	vars = NULL;
	marks = NULL;
	pool_count = pool_cap = func_count = decl_count = decl_cap = var_count = var_cap = mark_count = mark_cap = 0;
}

ErrorCode generate_program(AstNode *node) {
	if (node == NULL || node->kind != NODE_PROGRAM) {
		return ERR_INTERNAL_COMPILER;
	}
	const AstNode *prog = node;
	const char *main_name = NULL;

	for (int i = 0; i < prog->ast.program_node.function_count; i++) {
		const AstNode *fn = prog->ast.program_node.functions[i];
		func_add(fn, TYPE_UNKNOWN);
		if (fn->ast.func_node.is_main) {
			main_name = fn->ast.func_node.name;
		}
	}
	for (int i = 0; i < prog->ast.program_node.extension_count; i++) {
		const AstNode *ext = prog->ast.program_node.extensions[i];
		for (int j = 0; j < ext->ast.extension_node.method_count; j++) {
			func_add(ext->ast.extension_node.methods[j], ext->ast.extension_node.receiver_type);
		}
	}
	if (main_name == NULL || gen_err != ERR_OK) {
		generator_cleanup();
		return ERR_INTERNAL_COMPILER;
	}

	emit(".IFJcode26");
	emit("DEFVAR GF@_discard");
	emit("DEFVAR GF@_a");
	emit("DEFVAR GF@_b");
	emit("DEFVAR GF@_r");
	emit("CREATEFRAME");
	emit("CALL $fn_%s", main_name);
	emit("EXIT int@0");

	fputs(RUNTIME, gen_out);

	char label[300];
	for (int i = 0; i < prog->ast.program_node.function_count && gen_err == ERR_OK; i++) {
		const AstNode *fn = prog->ast.program_node.functions[i];
		snprintf(label, sizeof label, "$fn_%s", fn->ast.func_node.name);
		gen_function(fn, label, NULL, TYPE_UNKNOWN);
	}
	for (int i = 0; i < prog->ast.program_node.extension_count && gen_err == ERR_OK; i++) {
		const AstNode *ext = prog->ast.program_node.extensions[i];
		for (int j = 0; j < ext->ast.extension_node.method_count && gen_err == ERR_OK; j++) {
			const AstNode *m = ext->ast.extension_node.methods[j];
			snprintf(label, sizeof label, "$ext_%s_%s", type_label_name(ext->ast.extension_node.receiver_type),
			         m->ast.func_node.name);
			gen_function(m, label, ext->ast.extension_node.receiver_name, ext->ast.extension_node.receiver_type);
		}
	}

	ErrorCode err = gen_err;
	generator_cleanup();
	return err;
}