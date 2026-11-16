/* Debug tool: parses a program from stdin and prints its AST as an indented s-expression.
 * Every expression node is printed with its inferred type, e.g. (+ a b):Int */
#include "ast.h"
#include "builtins.h"
#include "parser.h"
#include <stdio.h>

static const char *OPS[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "||", "neg", "!"};
static const char *TY[] = {"Int", "Double", "String", "Boolean", "Unit", "?"};
#define T(n) TY[(n)->type]
#define TT(t) TY[t]

static void ind(int d) { for (int i = 0; i < d; i++) printf("  "); }
static void node(const AstNode *n, int d);

static void list(AstNode *const *a, int cnt, const int *sizes, int lists, int d) {
	int k = 0;
	for (int l = 0; l < (lists ? lists : 1); l++) {
		int sz = lists ? sizes[l] : cnt;
		printf(" [");
		for (int i = 0; i < sz; i++, k++) { if (i) printf(", "); node(a[k], -1); }
		printf("]");
	}
	(void)d;
}

/* d < 0 = inline (expressions), d >= 0 = statement with indentation */
static void node(const AstNode *n, int d) {
	if (!n) { printf("NULL"); return; }
	int in = d < 0;
	if (!in) ind(d);
	switch (n->kind) {
	case NODE_LIT_INT: printf("%lld", n->ast.literal.int_val); break;
	case NODE_LIT_DOUBLE: printf("%g", n->ast.literal.double_val); break;
	case NODE_LIT_STRING: printf("\"%s\"", n->ast.literal.str_val); break;
	case NODE_LIT_BOOL: printf(n->ast.literal.bool_val ? "true" : "false"); break;
	case NODE_LIT_UNIT: printf("()"); break;
	case NODE_ID: printf("%s:%s", n->ast.identifier.name, T(n)); break;
	case NODE_UNARY_OP: printf("(%s ", OPS[n->ast.unary.op]); node(n->ast.unary.operand, -1); printf("):%s", T(n)); break;
	case NODE_BINARY_OP:
		printf("("); node(n->ast.binary.left, -1); printf(" %s ", OPS[n->ast.binary.op]);
		node(n->ast.binary.right, -1); printf("):%s", T(n)); break;
	case NODE_CALL: printf("call %s", n->ast.call_node.name);
		list(n->ast.call_node.args, n->ast.call_node.arg_count, n->ast.call_node.list_sizes, n->ast.call_node.list_count, 0);
		printf(":%s", T(n)); break;
	case NODE_MEMBER: printf("member("); node(n->ast.member_node.receiver, -1);
		printf(").%s%s", n->ast.member_node.member_name, n->ast.member_node.has_parens ? "()" : "");
		if (n->ast.member_node.has_parens) list(n->ast.member_node.args, n->ast.member_node.arg_count, n->ast.member_node.list_sizes, n->ast.member_node.list_count, 0);
		printf(":%s", T(n)); break;
	case NODE_ISTRING: printf("istring"); list(n->ast.istring_node.parts, n->ast.istring_node.part_count, NULL, 0, 0); break;
	case NODE_IF_EXPR:
		printf("if-expr("); node(n->ast.if_node.cond, -1); printf(" ? "); node(n->ast.if_node.then_branch, -1);
		printf(" : "); node(n->ast.if_node.else_branch, -1); printf("):%s", T(n)); break;
	case NODE_BLOCK:
		printf("block{\n");
		for (int i = 0; i < n->ast.block_node.block.count; i++) { node(n->ast.block_node.block.statements[i], (in ? 1 : d) + 1); printf("\n"); }
		ind(in ? 1 : d); printf("}"); break;
	case NODE_VAL_DEF: case NODE_VAR_DEF:
		printf("%s %s:%s = ", n->kind == NODE_VAL_DEF ? "val" : "var", n->ast.var_val.name, T(n));
		node(n->ast.var_val.initializer, -1); break;
	case NODE_ASSIGN: printf("assign %s = ", n->ast.assign.name); node(n->ast.assign.value, -1); break;
	case NODE_EXPR_STMT: printf("expr "); node(n->ast.expr_stmt.child, -1); break;
	case NODE_RETURN: printf("return "); node(n->ast.expr_stmt.child, -1); break;
	case NODE_IF_STMT:
		printf("if "); node(n->ast.if_node.cond, -1); printf("\n"); node(n->ast.if_node.then_branch, d + 1);
		if (n->ast.if_node.else_branch) { printf("\n"); ind(d); printf("else\n"); node(n->ast.if_node.else_branch, d + 1); } break;
	case NODE_WHILE: printf("while "); node(n->ast.while_node.cond, -1); printf("\n"); node(n->ast.while_node.body, d + 1); break;
	case NODE_FOR:
		printf("for %s <- ", n->ast.for_node.var_name); node(n->ast.for_node.from, -1);
		printf(n->ast.for_node.is_until ? " until " : " to "); node(n->ast.for_node.to, -1);
		if (n->ast.for_node.by) { printf(" by "); node(n->ast.for_node.by, -1); }
		for (int i = 0; i < n->ast.for_node.guard_count; i++) { printf(" if "); node(n->ast.for_node.guards[i], -1); }
		printf("\n"); node(n->ast.for_node.body, d + 1); break;
	case NODE_PARAM: printf("%s:%s", n->ast.param_node.name, TT(n->ast.param_node.type)); break;
	case NODE_FUNC_DEF:
		printf("%sdef %s", n->ast.func_node.is_main ? "@main " : "", n->ast.func_node.name);
		list(n->ast.func_node.params, n->ast.func_node.param_count, n->ast.func_node.list_sizes, n->ast.func_node.list_count, 0);
		printf(": %s\n", TT(n->ast.func_node.return_type)); node(n->ast.func_node.body, d + 1); break;
	case NODE_EXTENSION:
		printf("extension(%s: %s)\n", n->ast.extension_node.receiver_name, TT(n->ast.extension_node.receiver_type));
		for (int i = 0; i < n->ast.extension_node.method_count; i++) { node(n->ast.extension_node.methods[i], d + 1); printf("\n"); } break;
	case NODE_PROGRAM:
		for (int i = 0; i < n->ast.program_node.function_count; i++) { node(n->ast.program_node.functions[i], 0); printf("\n"); }
		for (int i = 0; i < n->ast.program_node.extension_count; i++) { node(n->ast.program_node.extensions[i], 0); printf("\n"); } break;
	}
}

int main(void) {
	Scanner sc;
	scanner_init(&sc);
	SymTable g;
	symtable_init(&g, NULL);
	register_builtins(&g);
	Parser p; parser_init(&p, &sc, &g);
	AstNode *prog = NULL;
	ErrorCode e = prolog(&p, &prog);
	if (e == ERR_OK) { node(prog, 0); ast_free(prog); } else printf("ERROR %d\n", e);
	symtable_free(&g);
	parser_free(&p);
	scanner_free(&sc);
	return e;
}
