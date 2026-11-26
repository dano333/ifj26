/*
 * Project: IFJ26
 * Authors: Jakub Krčmárik (xkrcmaj00)
 * Description: expression parser - precedence syntax analysis + AST construction
 */

#include "expr.h"
#include "ast.h"
#include "semantics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/*  Precedence table                                                         */
/* ------------------------------------------------------------------------- */

typedef enum {
	PT_NOT,    /* ! (prefix)   */
	PT_NEG,    /* - (prefix)   */
	PT_MULDIV, /* * /          */
	PT_ADDSUB, /* + -          */
	PT_REL,    /* < <= > >=    */
	PT_EQ,     /* == !=        */
	PT_AND,    /* &&           */
	PT_OR,     /* ||           */
	PT_LPAREN, /* (            */
	PT_RPAREN, /* )            */
	PT_DOLLAR, /* end of expr  */
	PT_COUNT
} PTerm;

/* '<' shift, '>' reduce, '=' shift ')' and collapse "( E )", 'x' error, 'a' accept.
 * row = terminal on top of the stack, column = input terminal. */
static const char PREC[PT_COUNT][PT_COUNT] = {
    /*          !    -u   */ /* *  /   +  -   rel  eq   &&   ||   (    )    $ */
    /* !   */ {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '>'},
    /* -u  */ {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '>'},
    /* *,/ */ {'<', '<', '>', '>', '>', '>', '>', '>', '<', '>', '>'},
    /* +,- */ {'<', '<', '<', '>', '>', '>', '>', '>', '<', '>', '>'},
    /* rel */ {'<', '<', '<', '<', 'x', '>', '>', '>', '<', '>', '>'},
    /* ==  */ {'<', '<', '<', '<', '<', 'x', '>', '>', '<', '>', '>'},
    /* &&  */ {'<', '<', '<', '<', '<', '<', '>', '>', '<', '>', '>'},
    /* ||  */ {'<', '<', '<', '<', '<', '<', '<', '>', '<', '>', '>'},
    /* (   */ {'<', '<', '<', '<', '<', '<', '<', '<', '<', '=', 'x'},
    /* )   */ {'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'},
    /* $   */ {'<', '<', '<', '<', '<', '<', '<', '<', '<', 'x', 'a'},
};

static ErrorCode syntax_error(Parser *parser, const char *where) {
	fprintf(stderr, "\n[PARSER ERROR] %s() failed at line %d, col %d (Token Type: %d)\n", where,
	        parser->current_token.line, parser->current_token.column, parser->current_token.type);
	return ERR_PARSING;
}

static ErrorCode advance(Parser *parser) { return compare_token(parser, parser->current_token.type); }

static bool is_num(DataType t) { return t == TYPE_INT || t == TYPE_DOUBLE; }

bool is_expr_start(TokenType t) {
	switch (t) {
	case TOKEN_ID_IDENT:
	case TOKEN_LIT_INT:
	case TOKEN_LIT_DOUBLE:
	case TOKEN_LIT_STRING:
	case TOKEN_KW_TRUE:
	case TOKEN_KW_FALSE:
	case TOKEN_KW_IF:
	case TOKEN_INTERP_START:
	case TOKEN_SE_LPAREN:
	case TOKEN_OP_MINUS:
	case TOKEN_OP_NOT:
		return true;
	default:
		return false;
	}
}

/* tokens that begin an operand by themselves (no prefix operator, no parenthesis) */
static bool is_operand_start(TokenType t) {
	return is_expr_start(t) && t != TOKEN_SE_LPAREN && t != TOKEN_OP_MINUS && t != TOKEN_OP_NOT;
}

/* type of a value-producing node; a block yields the type of its last statement */
static DataType node_type(const AstNode *n) {
	if (n == NULL) {
		return TYPE_UNKNOWN;
	}
	if (n->kind == NODE_BLOCK) {
		int cnt = n->ast.block_node.block.count;
		return cnt > 0 ? node_type(n->ast.block_node.block.statements[cnt - 1]) : TYPE_UNIT;
	}
	if (n->kind == NODE_EXPR_STMT) {
		return node_type(n->ast.expr_stmt.child);
	}
	return n->type;
}

void interp_free_parts(AstNode **parts, int count) {
	if (parts == NULL) {
		return;
	}
	for (int i = 0; i < count; i++) {
		ast_free(parts[i]);
	}
	free(parts);
}

typedef struct {
	AstNode **nodes;
	DataType *types;
	int count;
	int cap;
	int *sizes; /* number of arguments in every parenthesised list */
	int lists;
} ArgList;

static void arglist_free(ArgList *a) {
	interp_free_parts(a->nodes, a->count);
	free(a->types);
	free(a->sizes);
	a->nodes = NULL;
	a->types = NULL;
	a->sizes = NULL;
	a->count = a->lists = a->cap = 0;
}

/* on failure the node is NOT consumed */
static ErrorCode arglist_add(ArgList *a, AstNode *node, DataType type) {
	if (a->count == a->cap) {
		int new_cap = a->cap == 0 ? 4 : a->cap * 2;
		AstNode **n = realloc(a->nodes, new_cap * sizeof(AstNode *));
		if (n == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		a->nodes = n;
		DataType *t = realloc(a->types, new_cap * sizeof(DataType));
		if (t == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		a->types = t;
		a->cap = new_cap;
	}
	a->nodes[a->count] = node;
	a->types[a->count] = type;
	a->count++;
	return ERR_OK;
}

static ErrorCode parse_arg_lists(Parser *parser, ArgList *a) {
	ErrorCode err;
	while (parser->current_token.type == TOKEN_SE_LPAREN) {
		if ((err = advance(parser)) != ERR_OK) {
			return err;
		}
		int before = a->count;
		if (parser->current_token.type != TOKEN_SE_RPAREN) {
			for (;;) {
				AstNode *arg = NULL;
				DataType t = TYPE_UNKNOWN;
				if ((err = parse_expr(parser, &arg, &t)) != ERR_OK) {
					return err;
				}
				if ((err = arglist_add(a, arg, t)) != ERR_OK) {
					ast_free(arg);
					return err;
				}
				if (parser->current_token.type == TOKEN_SE_COMMA) {
					if ((err = advance(parser)) != ERR_OK) {
						return err;
					}
					continue;
				}
				break;
			}
		}
		if (parser->current_token.type != TOKEN_SE_RPAREN) {
			return syntax_error(parser, "parse_arg_lists");
		}
		if ((err = advance(parser)) != ERR_OK) {
			return err;
		}
		int *sizes = realloc(a->sizes, (a->lists + 1) * sizeof(int));
		if (sizes == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		a->sizes = sizes;
		a->sizes[a->lists++] = a->count - before;
	}
	return ERR_OK;
}

static ErrorCode parse_identifier(Parser *parser, AstNode **out, DataType *type) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	char *name = strdup(parser->current_token.value.string_value);
	if (name == NULL) {
		return ERR_INTERNAL_COMPILER;
	}

	Symbol *sym = symtable_lookup(parser->current_scope, name);
	if (sym == NULL) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] Undefined identifier or function: '%s' at line %d\n", name, line);
		free(name);
		return ERR_SEMANTIC_UNDEFINED;
	}
	bool is_func = (sym->kind == SYMBOL_FUNC);
	DataType sym_type = sym->type;

	if ((err = advance(parser)) != ERR_OK) {
		free(name);
		return err;
	}

	if (!is_func) {
		AstNode *id = ast_new_identifier(name, line, col);
		if (id == NULL) {
			free(name);
			return ERR_INTERNAL_COMPILER;
		}
		id->type = sym_type;
		*out = id;
		*type = sym_type;
		return ERR_OK;
	}

	/* function call, a function without parentheses is a call with no argument lists */
	ArgList args = {0};
	DataType ret = TYPE_UNKNOWN;
	if ((err = parse_arg_lists(parser, &args)) != ERR_OK ||
	    (err = resolve_function(parser, symtable_lookup(parser->current_scope, name))) != ERR_OK ||
	    (err = validate_function_call(parser->current_scope, name, args.types, args.count, args.sizes, args.lists,
	                                  &ret)) != ERR_OK) {
		arglist_free(&args);
		free(name);
		return err;
	}

	AstNode *call = ast_new_call(name, args.nodes, args.count, args.sizes, args.lists, line, col);
	if (call == NULL) {
		arglist_free(&args);
		free(name);
		return ERR_INTERNAL_COMPILER;
	}
	free(args.types); /* nodes and sizes now belong to the call node */
	call->type = ret;
	*out = call;
	*type = ret;
	return ERR_OK;
}

typedef struct {
	const char *name;
	DataType receiver; /* TYPE_UNKNOWN = any value */
	DataType result;
} Member;

static const Member MEMBERS[] = {
    {"toString", TYPE_UNKNOWN, TYPE_STRING},
    {"toInt", TYPE_DOUBLE, TYPE_INT},
    {"floor", TYPE_DOUBLE, TYPE_DOUBLE},
    {"length", TYPE_STRING, TYPE_INT},
};

static ErrorCode builtin_member(DataType recv, const char *name, DataType *ret) {
	for (size_t i = 0; i < sizeof MEMBERS / sizeof MEMBERS[0]; i++) {
		const Member *m = &MEMBERS[i];
		if (strcmp(m->name, name) != 0) {
			continue;
		}
		if (m->receiver != TYPE_UNKNOWN && recv != TYPE_UNKNOWN && recv != m->receiver) {
			fprintf(stderr, "\n[SEMANTIC DEBUG] %s has no member '%s'\n", type_name(recv), name);
			return ERR_SEMANTIC_COMPATIBILITY;
		}
		*ret = m->result;
		return ERR_OK;
	}
	if (recv != TYPE_UNKNOWN) {
		fprintf(stderr, "\n[SEMANTIC DEBUG] %s has no member '%s'\n", type_name(recv), name);
	}
	return ERR_SEMANTIC_UNDEFINED;
}

static ErrorCode call_with_receiver(Parser *parser, Symbol *sym, const char *name, DataType recv, const ArgList *args,
                                    DataType *ret) {
	int n = args->count + 1;
	int nl = args->lists > 0 ? args->lists : 1;
	DataType *types = malloc((size_t)n * sizeof(DataType));
	int *sizes = calloc((size_t)nl, sizeof(int));
	ErrorCode err = ERR_INTERNAL_COMPILER;

	if (types != NULL && sizes != NULL) {
		types[0] = recv;
		memcpy(types + 1, args->types, (size_t)args->count * sizeof(DataType));
		memcpy(sizes, args->sizes, (size_t)args->lists * sizeof(int));
		sizes[0]++;
		err = resolve_function(parser, sym);
		if (err == ERR_OK) {
			err = validate_function_call(parser->current_scope, name, types, n, sizes, nl, ret);
		}
	}
	free(types);
	free(sizes);
	return err;
}

static ErrorCode parse_member(Parser *parser, AstNode **recv_node, DataType *recv_type) {
	ErrorCode err;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	ArgList args = {0};
	char *name = NULL;
	DataType ret = TYPE_UNKNOWN;

	if ((err = advance(parser)) != ERR_OK) {
		return err;
	}
	if (parser->current_token.type != TOKEN_ID_IDENT) {
		return syntax_error(parser, "parse_member");
	}
	if ((name = strdup(parser->current_token.value.string_value)) == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if ((err = advance(parser)) != ERR_OK) {
		goto fail;
	}

	bool has_parens = (parser->current_token.type == TOKEN_SE_LPAREN);
	if (has_parens && (err = parse_arg_lists(parser, &args)) != ERR_OK) {
		goto fail;
	}

	char key[256];
	mangle_method(key, sizeof key, *recv_type, name);
	Symbol *msym = symtable_lookup(parser->current_scope, key);
	Symbol *sym = symtable_lookup(parser->current_scope, name);

	if (msym != NULL && msym->kind == SYMBOL_FUNC) {
		if ((err = resolve_function(parser, msym)) == ERR_OK) {
			err = validate_function_call(parser->current_scope, key, args.types, args.count, args.sizes, args.lists,
			                             &ret);
		}
	} else if (sym != NULL && sym->kind == SYMBOL_FUNC) {
		err = call_with_receiver(parser, sym, name, *recv_type, &args, &ret);
	} else if (!has_parens) {
		err = builtin_member(*recv_type, name, &ret);
	} else {
		err = ERR_SEMANTIC_UNDEFINED;
	}
	if (err != ERR_OK) {
		goto fail;
	}

	AstNode *member =
	    ast_new_member(*recv_node, name, args.nodes, args.count, args.sizes, args.lists, has_parens, line, col);
	if (member == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}
	free(args.types);
	member->type = ret;
	*recv_node = member;
	*recv_type = ret;
	return ERR_OK;

fail:
	arglist_free(&args);
	free(name);
	return err;
}

static ErrorCode parse_if_expr(Parser *parser, AstNode **out, DataType *type) {
	AstNode *n = NULL;
	ErrorCode err = if_control(parser, &n);
	if (err != ERR_OK) {
		return err;
	}

	DataType tt = node_type(n->ast.if_node.then_branch);
	DataType et = node_type(n->ast.if_node.else_branch);
	DataType rt;
	if (tt == TYPE_UNKNOWN) {
		rt = et;
	} else if (et == TYPE_UNKNOWN || tt == et) {
		rt = tt;
	} else if (is_num(tt) && is_num(et)) {
		rt = TYPE_DOUBLE;
	} else {
		fprintf(stderr, "\n[SEMANTIC DEBUG] if-expression branches differ: %d vs %d\n", tt, et);
		ast_free(n);
		return ERR_SEMANTIC_COMPATIBILITY;
	}
	n->type = rt;
	*out = n;
	*type = rt;
	return ERR_OK;
}

static ErrorCode parse_primary(Parser *parser, AstNode **out, DataType *type) {
	ErrorCode err;
	TokenType t = parser->current_token.type;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	AstNode *n = NULL;

	switch (t) {
	case TOKEN_LIT_INT:
		n = ast_new_literal_int(parser->current_token.value.int_value, line, col);
		break;
	case TOKEN_LIT_DOUBLE:
		n = ast_new_literal_double(parser->current_token.value.double_value, line, col);
		break;
	case TOKEN_LIT_STRING: {
		char *s = strdup(parser->current_token.value.string_value);
		if (s == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		n = ast_new_literal_string(s, line, col);
		if (n == NULL) {
			free(s);
		}
		break;
	}
	case TOKEN_KW_TRUE:
		n = ast_new_literal_bool(true, line, col);
		break;
	case TOKEN_KW_FALSE:
		n = ast_new_literal_bool(false, line, col);
		break;
	case TOKEN_INTERP_START:
		return interp_string(parser, out, type);
	case TOKEN_KW_IF:
		return parse_if_expr(parser, out, type);
	case TOKEN_ID_IDENT:
		return parse_identifier(parser, out, type);
	default:
		return syntax_error(parser, "parse_primary");
	}

	if (n == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	if ((err = advance(parser)) != ERR_OK) {
		ast_free(n);
		return err;
	}
	*out = n;
	*type = n->type;
	return ERR_OK;
}

static ErrorCode interp_part(Parser *parser, AstNode **part) {
	ErrorCode err;
	TokenType t = parser->current_token.type;
	int line = parser->current_token.line;
	int col = parser->current_token.column;
	const char *text = parser->current_token.value.string_value;

	if (t == TOKEN_INTERP_EXPR_START) {
		DataType type;
		if ((err = advance(parser)) != ERR_OK || (err = parse_expr(parser, part, &type)) != ERR_OK) {
			return err;
		}
		if ((err = compare_token(parser, TOKEN_INTERP_EXPR_END)) != ERR_OK) {
			ast_free(*part);
			*part = NULL;
		}
		return err;
	}
	if (t != TOKEN_INTERP_TEXT && t != TOKEN_INTERP_ID) {
		return syntax_error(parser, "interp_string");
	}

	Symbol *sym = NULL;
	if (t == TOKEN_INTERP_ID && (sym = symtable_lookup(parser->current_scope, text)) == NULL) {
		return ERR_SEMANTIC_UNDEFINED;
	}
	char *copy = strdup(text != NULL ? text : "");
	if (copy == NULL) {
		return ERR_INTERNAL_COMPILER;
	}
	*part = (t == TOKEN_INTERP_TEXT) ? ast_new_literal_string(copy, line, col) : ast_new_identifier(copy, line, col);
	if (*part == NULL) {
		free(copy);
		return ERR_INTERNAL_COMPILER;
	}
	if (sym != NULL) {
		(*part)->type = sym->type;
	}
	if ((err = advance(parser)) != ERR_OK) {
		ast_free(*part);
		*part = NULL;
	}
	return err;
}

ErrorCode interp_string(Parser *parser, AstNode **out_node, DataType *out_type) {
	ErrorCode err;
	AstNode **parts = NULL;
	int count = 0;
	int line = parser->current_token.line;
	int col = parser->current_token.column;

	if ((err = compare_token(parser, TOKEN_INTERP_START)) != ERR_OK) {
		return err;
	}

	while (parser->current_token.type != TOKEN_INTERP_END) {
		AstNode *part = NULL;
		AstNode **tmp = NULL;

		if ((err = interp_part(parser, &part)) != ERR_OK) {
			goto fail;
		}
		if ((tmp = realloc(parts, (size_t)(count + 1) * sizeof(AstNode *))) == NULL) {
			ast_free(part);
			err = ERR_INTERNAL_COMPILER;
			goto fail;
		}
		parts = tmp;
		parts[count++] = part;
	}

	if ((err = compare_token(parser, TOKEN_INTERP_END)) != ERR_OK) {
		goto fail;
	}
	if ((*out_node = ast_new_istring(parts, count, line, col)) == NULL) {
		err = ERR_INTERNAL_COMPILER;
		goto fail;
	}
	(*out_node)->type = TYPE_STRING;
	*out_type = TYPE_STRING;
	return ERR_OK;

fail:
	interp_free_parts(parts, count);
	return err;
}

static ErrorCode check_binary(OpType op, DataType l, DataType r, DataType *out) {
	bool unk = (l == TYPE_UNKNOWN || r == TYPE_UNKNOWN);

	switch (op) {
	case OP_ADD:
		if (l == TYPE_STRING && r == TYPE_STRING) {
			*out = TYPE_STRING;
			return ERR_OK;
		}
		/* fallthrough */
	case OP_SUB:
	case OP_MUL:
	case OP_DIV:
	case OP_MOD:
		if (unk) {
			*out = TYPE_UNKNOWN;
			return ERR_OK;
		}
		if (is_num(l) && is_num(r)) {
			*out = (l == TYPE_DOUBLE || r == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
			return ERR_OK;
		}
		break;

	case OP_LT:
	case OP_LE:
	case OP_GT:
	case OP_GE:
		if (unk || (is_num(l) && is_num(r)) || (l == TYPE_STRING && r == TYPE_STRING)) {
			*out = TYPE_BOOLEAN;
			return ERR_OK;
		}
		break;

	case OP_EQ:
	case OP_NE:
		if (unk || l == r || (is_num(l) && is_num(r))) {
			*out = TYPE_BOOLEAN;
			return ERR_OK;
		}
		break;

	case OP_AND:
	case OP_OR:
		if ((l == TYPE_BOOLEAN || l == TYPE_UNKNOWN) && (r == TYPE_BOOLEAN || r == TYPE_UNKNOWN)) {
			*out = TYPE_BOOLEAN;
			return ERR_OK;
		}
		break;

	default:
		break;
	}

	fprintf(stderr, "\n[SEMANTIC DEBUG] invalid operand types for operator %d: %d and %d\n", op, l, r);
	return ERR_SEMANTIC_COMPATIBILITY;
}

static ErrorCode check_unary(OpType op, DataType t, DataType *out) {
	if (op == OP_NEG && (is_num(t) || t == TYPE_UNKNOWN)) {
		*out = t;
		return ERR_OK;
	}
	if (op == OP_NOT && (t == TYPE_BOOLEAN || t == TYPE_UNKNOWN)) {
		*out = TYPE_BOOLEAN;
		return ERR_OK;
	}
	fprintf(stderr, "\n[SEMANTIC DEBUG] invalid operand type for unary operator %d: %d\n", op, t);
	return ERR_SEMANTIC_COMPATIBILITY;
}

static OpType token_to_op(TokenType t) {
	switch (t) {
	case TOKEN_OP_PLUS:
		return OP_ADD;
	case TOKEN_OP_MINUS:
		return OP_SUB;
	case TOKEN_OP_MUL:
		return OP_MUL;
	case TOKEN_OP_DIV:
		return OP_DIV;
	case TOKEN_COM_LT:
		return OP_LT;
	case TOKEN_COM_LE:
		return OP_LE;
	case TOKEN_COM_GT:
		return OP_GT;
	case TOKEN_COM_GE:
		return OP_GE;
	case TOKEN_COM_EQ:
		return OP_EQ;
	case TOKEN_COM_NE:
		return OP_NE;
	case TOKEN_OP_AND:
		return OP_AND;
	default:
		return OP_OR;
	}
}

typedef struct {
	bool is_term;  /* terminal (operator, parenthesis, $) or nonterminal E */
	PTerm term;    /* terminal kind */
	TokenType tok; /* original token of the operator */
	AstNode *node; /* AST of E */
	DataType type; /* type of E */
	int line, col;
} PItem;

typedef struct {
	PItem *items;
	int size;
	int cap;
} PStack;

static ErrorCode ps_push(PStack *s, PItem item) {
	if (s->size == s->cap) {
		int new_cap = s->cap == 0 ? 16 : s->cap * 2;
		PItem *n = realloc(s->items, new_cap * sizeof(PItem));
		if (n == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		s->items = n;
		s->cap = new_cap;
	}
	s->items[s->size++] = item;
	return ERR_OK;
}

/* frees every AST still owned by the stack */
static void ps_free(PStack *s) {
	for (int i = 0; i < s->size; i++) {
		if (!s->items[i].is_term) {
			ast_free(s->items[i].node);
		}
	}
	free(s->items);
	s->items = NULL;
	s->size = s->cap = 0;
}

static const PItem *top_terminal(const PStack *s) {
	for (int i = s->size - 1; i >= 0; i--) {
		if (s->items[i].is_term) {
			return &s->items[i];
		}
	}
	return NULL;
}

/* Reduces E -> op E  or  E -> E op E. On failure the stack is left untouched
 * (it still owns the nodes). */
static ErrorCode reduce(Parser *parser, PStack *s) {
	if (s->size < 3 || s->items[s->size - 1].is_term || !s->items[s->size - 2].is_term) {
		return syntax_error(parser, "reduce");
	}
	PItem *rhs = &s->items[s->size - 1];
	PItem *op = &s->items[s->size - 2];
	AstNode *node = NULL;
	DataType rt;
	ErrorCode err;

	if (op->term == PT_NOT || op->term == PT_NEG) {
		OpType o = (op->term == PT_NOT) ? OP_NOT : OP_NEG;
		if ((err = check_unary(o, rhs->type, &rt)) != ERR_OK) {
			return err;
		}
		node = ast_new_unary(o, rhs->node, op->line, op->col);
		if (node == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		s->size -= 2;
	} else if (op->term >= PT_MULDIV && op->term <= PT_OR) {
		if (s->items[s->size - 3].is_term) {
			return syntax_error(parser, "reduce");
		}
		PItem *lhs = &s->items[s->size - 3];
		OpType o = token_to_op(op->tok);
		if ((err = check_binary(o, lhs->type, rhs->type, &rt)) != ERR_OK) {
			return err;
		}
		node = ast_new_binary(o, lhs->node, rhs->node, op->line, op->col);
		if (node == NULL) {
			return ERR_INTERNAL_COMPILER;
		}
		s->size -= 3;
	} else {
		return syntax_error(parser, "reduce");
	}

	node->type = rt;
	PItem e = {.is_term = false, .node = node, .type = rt, .line = node->line, .col = node->column};
	s->items[s->size++] = e; /* capacity is guaranteed, we just removed at least two items */
	return ERR_OK;
}

/* maps the current token to a precedence terminal; false = token not allowed here */
static bool classify(TokenType t, bool prev_operand, int depth, PTerm *out) {
	if (!prev_operand) {
		switch (t) {
		case TOKEN_OP_NOT:
			*out = PT_NOT;
			return true;
		case TOKEN_OP_MINUS:
			*out = PT_NEG;
			return true;
		case TOKEN_SE_LPAREN:
			*out = PT_LPAREN;
			return true;
		default:
			return false;
		}
	}

	switch (t) {
	case TOKEN_OP_PLUS:
	case TOKEN_OP_MINUS:
		*out = PT_ADDSUB;
		break;
	case TOKEN_OP_MUL:
	case TOKEN_OP_DIV:
		*out = PT_MULDIV;
		break;
	case TOKEN_COM_LT:
	case TOKEN_COM_LE:
	case TOKEN_COM_GT:
	case TOKEN_COM_GE:
		*out = PT_REL;
		break;
	case TOKEN_COM_EQ:
	case TOKEN_COM_NE:
		*out = PT_EQ;
		break;
	case TOKEN_OP_AND:
		*out = PT_AND;
		break;
	case TOKEN_OP_OR:
		*out = PT_OR;
		break;
	case TOKEN_SE_RPAREN:
		*out = (depth > 0) ? PT_RPAREN : PT_DOLLAR;
		break;
	default: /* anything else ends the expression */
		*out = PT_DOLLAR;
		break;
	}
	return true;
}

ErrorCode parse_expr(Parser *parser, AstNode **out_node, DataType *out_type) {
	ErrorCode err = ERR_OK;
	PStack st = {0};
	bool prev_operand = false; /* was the previous symbol an operand (E)? */
	int depth = 0;             /* open parentheses */

	*out_node = NULL;
	*out_type = TYPE_UNKNOWN;

	PItem dollar = {.is_term = true, .term = PT_DOLLAR};
	if ((err = ps_push(&st, dollar)) != ERR_OK) {
		return err;
	}

	for (;;) {
		TokenType tt = parser->current_token.type;
		int line = parser->current_token.line;
		int col = parser->current_token.column;

		/* postfix member access binds tighter than every operator */
		if (prev_operand && tt == TOKEN_SE_DOT) {
			PItem *top = &st.items[st.size - 1];
			if ((err = parse_member(parser, &top->node, &top->type)) != ERR_OK) {
				goto fail;
			}
			continue;
		}

		/* operand */
		if (!prev_operand && is_operand_start(tt)) {
			AstNode *n = NULL;
			DataType ty = TYPE_UNKNOWN;
			if ((err = parse_primary(parser, &n, &ty)) != ERR_OK) {
				goto fail;
			}
			PItem e = {.is_term = false, .node = n, .type = ty, .line = line, .col = col};
			if ((err = ps_push(&st, e)) != ERR_OK) {
				ast_free(n);
				goto fail;
			}
			prev_operand = true;
			continue;
		}

		/* empty parentheses "()" are the Unit value */
		if (!prev_operand && tt == TOKEN_SE_RPAREN && depth > 0 && st.items[st.size - 1].is_term &&
		    st.items[st.size - 1].term == PT_LPAREN) {
			AstNode *u = ast_new_literal_unit(st.items[st.size - 1].line, st.items[st.size - 1].col);
			if (u == NULL) {
				err = ERR_INTERNAL_COMPILER;
				goto fail;
			}
			st.size--; /* drop '(' */
			PItem e = {.is_term = false, .node = u, .type = TYPE_UNIT, .line = u->line, .col = u->column};
			if ((err = ps_push(&st, e)) != ERR_OK) {
				ast_free(u);
				goto fail;
			}
			if ((err = advance(parser)) != ERR_OK) {
				goto fail;
			}
			depth--;
			prev_operand = true;
			continue;
		}

		PTerm in;
		if (!classify(tt, prev_operand, depth, &in) || (in == PT_DOLLAR && !prev_operand)) {
			err = syntax_error(parser, "parse_expr");
			goto fail;
		}

		for (;;) {
			const PItem *t = top_terminal(&st);
			char rel = PREC[t->term][in];

			if (rel == '<') {
				PItem op = {.is_term = true, .term = in, .tok = tt, .line = line, .col = col};
				if ((err = ps_push(&st, op)) != ERR_OK) {
					goto fail;
				}
				if ((err = advance(parser)) != ERR_OK) {
					goto fail;
				}
				if (in == PT_LPAREN) {
					depth++;
				}
				prev_operand = false;
				break;
			} else if (rel == '=') { /* "( E )" */
				if (st.size < 2 || st.items[st.size - 1].is_term || !st.items[st.size - 2].is_term ||
				    st.items[st.size - 2].term != PT_LPAREN) {
					err = syntax_error(parser, "parse_expr");
					goto fail;
				}
				if ((err = advance(parser)) != ERR_OK) { /* ')' */
					goto fail;
				}
				PItem e = st.items[st.size - 1];
				st.size -= 2;
				st.items[st.size++] = e;
				depth--;
				prev_operand = true;
				break;
			} else if (rel == '>') {
				if ((err = reduce(parser, &st)) != ERR_OK) {
					goto fail;
				}
			} else if (rel == 'a') {
				goto done;
			} else {
				err = syntax_error(parser, "parse_expr");
				goto fail;
			}
		}
	}

done:
	if (st.size != 2 || st.items[1].is_term) {
		err = syntax_error(parser, "parse_expr");
		goto fail;
	}
	*out_node = st.items[1].node;
	*out_type = st.items[1].type;
	st.items[1].node = NULL; /* ownership moved to the caller */
	ps_free(&st);
	return ERR_OK;

fail:
	ps_free(&st);
	return err;
}