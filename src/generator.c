/*
 * Project: IFJ26
 * Authors: David Pavlištík (xpavlid00)
 * Description: generator
 */

#include "generator.h"
#include "symtable.h"
#include "ast.h"
#include <stdio.h>

static FILE *gen_out = NULL;
static int label_counter = 0;

void generator_init(FILE *out)
{
    gen_out = out;
    label_counter = 0;
}

//header  
static void gen_header(void)
{
    fprintf(gen_out, ".IFJcode26\n");
    fprintf(gen_out, "DEFVAR GF@_discard\n");
    fprintf(gen_out, "JUMP $main\n");
}

//generates main function
static void gen_main_func_start(void)
{
    fprintf(gen_out, "LABEL $main\n");
    fprintf(gen_out, "CREATEFRAME\n");
    fprintf(gen_out, "PUSHFRAME\n");
}

//generates user function
static void gen_func_start(const char *name)
{
    fprintf(gen_out, "LABEL $%s\n", name);
    fprintf(gen_out, "PUSHFRAME\n");
}

//end
static void gen_func_end(void)
{
    fprintf(gen_out, "POPFRAME\n");
    fprintf(gen_out, "RETURN\n");
}

static void generate_node(AstNode *node);

void generate_program(AstNode *node)
{
    if (node == NULL) return;
    generate_node(node);
}

//
static void generate_node(AstNode *node)
{
    if (node == NULL) return;

    switch (node->kind)
    {
        case NODE_PROGRAM:
            gen_header();
            for (int i = 0; i < node->ast.program_node.function_count; i++)
            {
                generate_node(node->ast.program_node.functions[i]);
            }
            for (int i = 0; i < node->ast.program_node.extension_count; i++)
            {
                generate_node(node->ast.program_node.extensions[i]);
            }
            break;

        case NODE_FUNC_DEF:
            if (node->ast.func_node.is_main)
            {
                gen_main_func_start();
            }
            else
            {
                gen_func_start(node->ast.func_node.name);
            }
            generate_node(node->ast.func_node.body);
            gen_func_end();
            break;

        case NODE_BLOCK:
            for (int i = 0; i < node->ast.block_node.block.count; i++)
            {
                generate_node(node->ast.block_node.block.statements[i]);
            }
            break;

        //literals
        case NODE_LIT_INT:
            fprintf(gen_out, "PUSHS int@%lld\n", node->ast.literal.int_val);
            break;

        case NODE_LIT_DOUBLE:
            fprintf(gen_out, "PUSHS float@%a\n", node->ast.literal.double_val);
            break;

        case NODE_LIT_STRING:
            fprintf(gen_out, "PUSHS string@%s\n", node->ast.literal.str_val);
            break;

        case NODE_LIT_BOOL:
            fprintf(gen_out, "PUSHS bool@%s\n", node->ast.literal.bool_val ? "true" : "false");
            break;


        case NODE_LIT_UNIT:
            fprintf(gen_out, "PUSHS nil@nil\n");
            break;

        //variable
        case NODE_ID:
            fprintf(gen_out, "PUSHS LF@%s\n", node->ast.identifier.name);
            break;

        case NODE_VAL_DEF:
        case NODE_VAR_DEF:
            if (node->ast.var_val.initializer != NULL)
            {
                generate_node(node->ast.var_val.initializer);
            }
            fprintf(gen_out, "DEFVAR LF@%s\n", node->ast.var_val.name);
            fprintf(gen_out, "POPS LF@%s\n", node->ast.var_val.name);
            break;

        case NODE_ASSIGN:
            generate_node(node->ast.assign.value);
            fprintf(gen_out, "POPS LF@%s\n", node->ast.assign.name);
            break;

        //expresions
        case NODE_EXPR_STMT:
            if (node->ast.expr_stmt.child != NULL)
            {
                generate_node(node->ast.expr_stmt.child);
                fprintf(gen_out, "POPS GF@_discard\n");
            }
            break;

        //operations
        //unary
        case NODE_UNARY_OP:
            generate_node(node->ast.unary.operand);
            switch (node->ast.unary.op)
            {
                case OP_NEG:
                    fprintf(gen_out, "PUSHS int@-1\n");
                    fprintf(gen_out, "MULS\n");
                    break;
                case OP_NOT:
                    fprintf(gen_out, "NOTS\n");
                    break;
                default:
                    break;
            }
            break;

        //binary
        case NODE_BINARY_OP:
            generate_node(node->ast.binary.left);   
            generate_node(node->ast.binary.right);  

            switch (node->ast.binary.op)
            {
                case OP_ADD: fprintf(gen_out, "ADDS\n"); break;
                case OP_SUB: fprintf(gen_out, "SUBS\n"); break;
                case OP_MUL: fprintf(gen_out, "MULS\n"); break;
                case OP_DIV:
                    if (node->type == TYPE_INT)
                    {
                        fprintf(gen_out, "IDIVS\n");
                    }
                    else
                    {
                        fprintf(gen_out, "DIVS\n");
                    }
                    break;
                case OP_LT:  fprintf(gen_out, "LTS\n");  break;
                case OP_GT:  fprintf(gen_out, "GTS\n");  break;
                case OP_EQ:  fprintf(gen_out, "EQS\n");  break;
                case OP_NE:  
                    fprintf(gen_out, "EQS\n");
                    fprintf(gen_out, "NOTS\n"); 
                    break;
                case OP_LE:
                    fprintf(gen_out, "GTS\n");
                    fprintf(gen_out, "NOTS\n");
                    break;
                case OP_GE:
                    fprintf(gen_out, "LTS\n");
                    fprintf(gen_out, "NOTS\n");
                    break;
                case OP_AND: fprintf(gen_out, "ANDS\n"); break;
                case OP_OR:  fprintf(gen_out, "ORS\n");  break;
                default: break;
            }
            break;

        //function call
        case NODE_CALL:
        {
            fprintf(gen_out, "CREATEFRAME\n");
            
            for (int i = 0; i < node->ast.call_node.arg_count; i++)
            {
                generate_node(node->ast.call_node.args[i]);
                fprintf(gen_out, "POPS TF@_p%d\n", i);
            }
            
            fprintf(gen_out, "CALL $%s\n", node->ast.call_node.name);
            break;
        }

        //conditions
        case NODE_IF_STMT:
        {
            int lbl = label_counter++;
            
            generate_node(node->ast.if_node.cond);
            fprintf(gen_out, "JUMPIFFALSE $else_%d\n", lbl);
            
            generate_node(node->ast.if_node.then_branch);
            fprintf(gen_out, "JUMP $endif_%d\n", lbl);
            
            fprintf(gen_out, "LABEL $else_%d\n", lbl);
            if (node->ast.if_node.else_branch != NULL)
            {
                generate_node(node->ast.if_node.else_branch);
            }
            
            fprintf(gen_out, "LABEL $endif_%d\n", lbl);
            break;
        }

        case NODE_IF_EXPR:
        {
            int lbl = label_counter++;
            char var_res[64];
            sprintf(var_res, "_if_res_%d", lbl);
            
            fprintf(gen_out, "DEFVAR LF@%s\n", var_res);

            generate_node(node->ast.if_node.cond);
            fprintf(gen_out, "JUMPIFFALSE $else_expr_%d\n", lbl);

            generate_node(node->ast.if_node.then_branch);
            fprintf(gen_out, "POPS LF@%s\n", var_res);
            fprintf(gen_out, "JUMP $endif_expr_%d\n", lbl);

            fprintf(gen_out, "LABEL $else_expr_%d\n", lbl);
            if (node->ast.if_node.else_branch != NULL)
            {
                generate_node(node->ast.if_node.else_branch);
                fprintf(gen_out, "POPS LF@%s\n", var_res);
            }

            fprintf(gen_out, "LABEL $endif_expr_%d\n", lbl);
            fprintf(gen_out, "PUSHS LF@%s\n", var_res);
            break;
        }

        //while cycle
        case NODE_WHILE:
        {
            int lbl = label_counter++;

            fprintf(gen_out, "LABEL $while_start_%d\n", lbl);
            generate_node(node->ast.while_node.cond);
            fprintf(gen_out, "JUMPIFFALSE $while_end_%d\n", lbl);
            
            generate_node(node->ast.while_node.body);
            fprintf(gen_out, "JUMP $while_start_%d\n", lbl);
            
            fprintf(gen_out, "LABEL $while_end_%d\n", lbl);
            break;
        }

        //for cycle
        case NODE_FOR:
        {
            int lbl = label_counter++;
            char *vname = node->ast.for_node.var_name;

            fprintf(gen_out, "DEFVAR LF@%s\n", vname);
            
            generate_node(node->ast.for_node.from);
            fprintf(gen_out, "POPS LF@%s\n", vname);

            char limit_var[64];
            sprintf(limit_var, "_for_limit_%d", lbl);
            fprintf(gen_out, "DEFVAR LF@%s\n", limit_var);
            generate_node(node->ast.for_node.to);
            fprintf(gen_out, "POPS LF@%s\n", limit_var);

            fprintf(gen_out, "LABEL $for_start_%d\n", lbl);

            fprintf(gen_out, "PUSHS LF@%s\n", vname);
            fprintf(gen_out, "PUSHS LF@%s\n", limit_var);

            if (node->ast.for_node.is_until)
            {
                fprintf(gen_out, "LTS\n"); 
            }
            else
            {
                fprintf(gen_out, "GTS\n");
                fprintf(gen_out, "NOTS\n");
            }
            
            fprintf(gen_out, "JUMPIFFALSE $for_end_%d\n", lbl);

            for (int g = 0; g < node->ast.for_node.guard_count; g++)
            {
                generate_node(node->ast.for_node.guards[g]);
                fprintf(gen_out, "JUMPIFFALSE $for_continue_%d\n", lbl);
            }

            generate_node(node->ast.for_node.body);

            fprintf(gen_out, "LABEL $for_continue_%d\n", lbl);
            
            fprintf(gen_out, "PUSHS LF@%s\n", vname);
            if (node->ast.for_node.by != NULL)
            {
                generate_node(node->ast.for_node.by);
            }
            else
            {
                fprintf(gen_out, "PUSHS int@1\n");
            }
            fprintf(gen_out, "ADDS\n");
            fprintf(gen_out, "POPS LF@%s\n", vname);

            fprintf(gen_out, "JUMP $for_start_%d\n", lbl);
            fprintf(gen_out, "LABEL $for_end_%d\n", lbl);
            break;
        }

        //return
        case NODE_RETURN:
            if (node->ast.expr_stmt.child != NULL)
            {
                generate_node(node->ast.expr_stmt.child);
            }
            gen_func_end();
            break;

        default:
            break;
    }
}