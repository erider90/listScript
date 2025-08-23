#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define debug(m,e) printf("%s:%d: %s:",__FILE__,__LINE__,m); print_obj(e,1); puts("");

/* --- Data Structures --- */

// Node types for the AST
typedef enum {
    DEF,
    ARGS,
    LIST,
    DATA,
    IF,
    SYMBOL,
    NUMBER,
    PRIMITIVE_OP,
    BOOLEAN,
    ERROR,
    FUNCTION_CALL
} NodeType;

// A Node represents a single element in our program's AST.
typedef struct Node {
    NodeType type;
    union {
        // For SYMBOL, PRIMITIVE_OP, and ERROR nodes
        char *name;
        // For NUMBER and BOOLEAN nodes
        long number;
        // For LIST, ARGS, DEF, DATA, IF, FUNCTION_CALL nodes
        struct {
            struct Node **children;
            int child_count;
        } compound;
    } value;
} Node;

// Forward declaration to fix the compiler warning
void print_node(Node *node);

// The Environment is a simple linked list mapping names to their Node values.
typedef struct Env {
    struct Env *next;
    char *name;
    struct Node *value;
} Env;

/* --- Core Functions --- */

// Input buffer for the tokenizer.
#define INPUT_MAX 1024
static char input_buffer[INPUT_MAX];
static int input_pos = 0;
#define SYMBOL_MAX 32
static char token[SYMBOL_MAX];

// Tokenizer helpers
int is_space(char x) { return x == ' ' || x == '\n' || x == '\t'; }
int is_parens(char x) { return x == '(' || x == ')'; }
int is_alpha(char x) { return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z'); }
int is_digit(char x) { return x >= '0' && x <= '9'; }

// Reads a line from stdin into the buffer
int read_line() {
    if (fgets(input_buffer, INPUT_MAX, stdin)) {
        input_pos = 0;
        return 1;
    }
    return 0;
}

char peek_char() {
    int i = input_pos;
    while (is_space(input_buffer[i])) {
        i++;
    }
    return input_buffer[i];
}

// Gets the next token from the buffer
int gettoken() {
    int index = 0;
    while (is_space(input_buffer[input_pos])) {
        input_pos++;
    }
    if (input_buffer[input_pos] == '\0' || input_buffer[input_pos] == EOF) return 0;
    if (is_parens(input_buffer[input_pos])) {
        token[index++] = input_buffer[input_pos++];
    } else {
        while (index < SYMBOL_MAX - 1 && input_buffer[input_pos] != '\0' && !is_space(input_buffer[input_pos]) && !is_parens(input_buffer[input_pos])) {
            token[index++] = input_buffer[input_pos++];
        }
    }
    token[index] = '\0';
    return 1;
}

// Node creation functions (memory allocation helpers)
Node *make_node(NodeType type) {
    Node *node = calloc(1, sizeof(Node));
    node->type = type;
    return node;
}

Node *make_number(long num) {
    Node *node = make_node(NUMBER);
    node->value.number = num;
    return node;
}

Node *make_symbol(char *name) {
    Node *node = make_node(SYMBOL);
    node->value.name = strdup(name);
    return node;
}

Node *make_primitive_op(char *name) {
    Node *node = make_node(PRIMITIVE_OP);
    node->value.name = strdup(name);
    return node;
}

Node *make_boolean(int value) {
    Node *node = make_node(BOOLEAN);
    node->value.number = value;
    return node;
}

Node *make_error(char *message) {
    Node *node = make_node(ERROR);
    node->value.name = strdup(message);
    return node;
}

Node *make_compound_node(NodeType type, int child_count) {
    Node *node = make_node(type);
    node->value.compound.child_count = child_count;
    node->value.compound.children = calloc(child_count, sizeof(Node *));
    return node;
}

Env *define(Env *env, char *name, Node *value) {
    Env *new_binding = calloc(1, sizeof(Env));
    new_binding->name = strdup(name);
    new_binding->value = value;
    new_binding->next = env;
    return new_binding;
}

Node *lookup(Env *env, char *name) {
    for (; env; env = env->next) {
        if (strcmp(env->name, name) == 0) return env->value;
    }
    return NULL;
}

// Forward declaration
Node *parse_expression();

// Parser for content inside parentheses
Node *parse_paren_content() {
    Node *content = make_compound_node(LIST, 0);
    while (gettoken() && token[0] != ')') {
        Node *next = parse_expression();
        if (next) {
            content->value.compound.children = realloc(content->value.compound.children, (content->value.compound.child_count + 1) * sizeof(Node*));
            content->value.compound.children[content->value.compound.child_count++] = next;
        }
    }
    return content;
}

Node *parse_expression() {
    if (strcmp(token, "def") == 0) {
        Node *def_node = make_compound_node(DEF, 3);
        // Child 0: function name
        gettoken();
        def_node->value.compound.children[0] = make_symbol(token);
        // Child 1: args list
        gettoken(); // consume 'args'
        gettoken(); // consume '('
        Node *args_list = make_compound_node(ARGS, 0);
        while(gettoken() && token[0] != ')') {
            args_list->value.compound.children = realloc(args_list->value.compound.children, (args_list->value.compound.child_count + 1) * sizeof(Node*));
            args_list->value.compound.children[args_list->value.compound.child_count++] = make_symbol(token);
        }
        def_node->value.compound.children[1] = args_list;
        // Child 2: body
        gettoken(); // consume 'list'
        gettoken(); // consume '('
        def_node->value.compound.children[2] = parse_paren_content();
        return def_node;
    } else if (strcmp(token, "if") == 0) {
        Node *if_node = make_compound_node(IF, 3);
        // Child 0: condition
        gettoken();
        if_node->value.compound.children[0] = parse_expression();
        // Child 1: true branch
        gettoken();
        if_node->value.compound.children[1] = parse_expression();
        // Child 2: false branch
        gettoken();
        if_node->value.compound.children[2] = parse_expression();
        return if_node;
    } else if (strcmp(token, "list") == 0) {
        Node *list_node = make_compound_node(LIST, 0);
        gettoken(); // consume '('
        while (gettoken() && token[0] != ')') {
            Node *next = parse_expression();
            if (next) {
                list_node->value.compound.children = realloc(list_node->value.compound.children, (list_node->value.compound.child_count + 1) * sizeof(Node*));
                list_node->value.compound.children[list_node->value.compound.child_count++] = next;
            }
        }
        return list_node;
    } else if (strcmp(token, "data") == 0) {
        Node *data_node = make_compound_node(DATA, 0);
        gettoken(); // consume '('
        while (gettoken() && token[0] != ')') {
            Node *next = parse_expression();
            if (next) {
                data_node->value.compound.children = realloc(data_node->value.compound.children, (data_node->value.compound.child_count + 1) * sizeof(Node*));
                data_node->value.compound.children[data_node->value.compound.child_count++] = next;
            }
        }
        return data_node;
    } else {
        if (peek_char() == '(') {
            Node *call_node = make_compound_node(FUNCTION_CALL, 0);
            call_node->value.compound.children = realloc(call_node->value.compound.children, sizeof(Node*));
            call_node->value.compound.children[call_node->value.compound.child_count++] = make_symbol(token);
            gettoken(); // consume '('
            Node *args_content = parse_paren_content();
            call_node->value.compound.children = realloc(call_node->value.compound.children, (call_node->value.compound.child_count + args_content->value.compound.child_count) * sizeof(Node*));
            for (int i = 0; i < args_content->value.compound.child_count; i++) {
                call_node->value.compound.children[call_node->value.compound.child_count++] = args_content->value.compound.children[i];
            }
            return call_node;
        } else {
            // Check if it's a number
            long num = strtol(token, NULL, 10);
            if (num != 0 || (strcmp(token, "0") == 0)) {
                return make_number(num);
            } else {
                return make_symbol(token);
            }
        }
    }
}

Node *eval(Node *expr, Env **env) {
    if (!expr) return NULL;
    switch (expr->type) {
        case SYMBOL: {
            Node *result = lookup(*env, expr->value.name);
            if (!result) {
                char error_msg[100];
                sprintf(error_msg, "Undefined symbol '%s'", expr->value.name);
                return make_error(error_msg);
            }
            return result;
        }
        case NUMBER:
        case BOOLEAN:
        case ERROR:
        case PRIMITIVE_OP: // Primitives are self-evaluating
            return expr;
        case LIST:
        case FUNCTION_CALL: {
            if (expr->value.compound.child_count == 0) return NULL;
            Node *op = expr->value.compound.children[0];
            
            if (op->type == SYMBOL) {
                Node *op_val = lookup(*env, op->value.name);

                if (op_val && op_val->type == PRIMITIVE_OP) {
                    // Handle standard binary primitives
                    if (expr->value.compound.child_count != 3) {
                        return make_error("Arity mismatch: Expected 2 arguments for primitive operator");
                    }
                    char *op_name = op_val->value.name;
                    Node *arg1 = eval(expr->value.compound.children[1], env);
                    Node *arg2 = eval(expr->value.compound.children[2], env);

                    if (arg1->type == ERROR) return arg1;
                    if (arg2->type == ERROR) return arg2;

                    // Ensure arguments are numbers
                    if (arg1->type != NUMBER || arg2->type != NUMBER) {
                        return make_error("Type error: Arguments must be numbers");
                    }

                    long num1 = arg1->value.number;
                    long num2 = arg2->value.number;

                    if (strcmp(op_name, "+") == 0) return make_number(num1 + num2);
                    else if (strcmp(op_name, "-") == 0) return make_number(num1 - num2);
                    else if (strcmp(op_name, "*") == 0) return make_number(num1 * num2);
                    else if (strcmp(op_name, "/") == 0) {
                        if (num2 == 0) {
                            return make_error("Division by zero");
                        }
                        return make_number(num1 / num2);
                    }
                    else if (strcmp(op_name, "<") == 0) return make_boolean(num1 < num2);
                    else if (strcmp(op_name, ">") == 0) return make_boolean(num1 > num2);
                    else if (strcmp(op_name, "eq?") == 0) return make_boolean(num1 == num2);
                } else if (op_val && op_val->type == DEF) {
                    // This is a user-defined function call
                    Node *args_node = op_val->value.compound.children[1];
                    Node *body_node = op_val->value.compound.children[2];
                    
                    // Check arity
                    if (args_node->value.compound.child_count != expr->value.compound.child_count - 1) {
                        return make_error("Arity mismatch in user-defined function");
                    }
                    
                    // Create a new local environment for the function call
                    Env *local_env = *env;

                    // Bind arguments to parameters
                    for (int i = 0; i < args_node->value.compound.child_count; i++) {
                        Node *param_name = args_node->value.compound.children[i];
                        Node *arg_value = eval(expr->value.compound.children[i+1], env);
                        if (arg_value->type == ERROR) return arg_value;
                        local_env = define(local_env, param_name->value.name, arg_value);
                    }

                    // Evaluate the function body within the local environment
                    return eval(body_node, &local_env);
                }
            } else if (op->type == IF) {
                // Handle 'if' special form
                return eval(op, env);
            }
            return make_error("Cannot apply a non-function or undefined operator");
        }
        case DEF: {
            char *name = expr->value.compound.children[0]->value.name;
            *env = define(*env, name, expr);
            return NULL;
        }
        case DATA:
            return expr; // Data is self-evaluating
        case IF: {
            Node *condition = expr->value.compound.children[0];
            Node *true_branch = expr->value.compound.children[1];
            Node *false_branch = expr->value.compound.children[2];
            
            Node *condition_result = eval(condition, env);
            if (condition_result->type == ERROR) return condition_result;
            
            if (condition_result && condition_result->type == BOOLEAN) {
                if (condition_result->value.number != 0) {
                    return eval(true_branch, env);
                } else {
                    return eval(false_branch, env);
                }
            } else {
                 return make_error("'if' condition must be a boolean");
            }
        }
        default:
            return make_error("Cannot evaluate expression of this type");
    }
}

void print_node(Node *node) {
    if (!node) {
        printf("nil");
        return;
    }
    switch(node->type) {
        case NUMBER:
            printf("%ld", node->value.number);
            break;
        case BOOLEAN:
            if (node->value.number == 1) printf("true"); else printf("false");
            break;
        case SYMBOL:
        case PRIMITIVE_OP:
            printf("%s", node->value.name);
            break;
        case ERROR:
            printf("Error: %s", node->value.name);
            break;
        case LIST:
            printf("list(");
            for (int i = 0; i < node->value.compound.child_count; i++) {
                print_node(node->value.compound.children[i]);
                if (i < node->value.compound.child_count - 1) {
                    printf(" ");
                }
            }
            printf(")");
            break;
        case FUNCTION_CALL:
            printf("func_call(");
            for (int i = 0; i < node->value.compound.child_count; i++) {
                print_node(node->value.compound.children[i]);
                if (i < node->value.compound.child_count - 1) {
                    printf(" ");
                }
            }
            printf(")");
            break;
        case DATA:
            printf("data(");
            for (int i = 0; i < node->value.compound.child_count; i++) {
                print_node(node->value.compound.children[i]);
                if (i < node->value.compound.child_count - 1) {
                    printf(" ");
                }
            }
            printf(")");
            break;
        default:
            printf("?");
    }
}

/* --- Main Loop --- */

int main() {
    // Set up initial environment with primitives
    Env *env = NULL;
    env = define(env, "+", make_primitive_op("+"));
    env = define(env, "-", make_primitive_op("-"));
    env = define(env, "*", make_primitive_op("*"));
    env = define(env, "/", make_primitive_op("/"));
    env = define(env, "<", make_primitive_op("<"));
    env = define(env, ">", make_primitive_op(">"));
    env = define(env, "eq?", make_primitive_op("eq?"));
    env = define(env, "write", make_primitive_op("write"));
    env = define(env, "true", make_boolean(1));
    env = define(env, "false", make_boolean(0));
    
    printf("ListScript ready.\n");
    while (1) {
        printf("-> ");
        if (!read_line()) break;
        if (!gettoken()) continue;
        
        if (strcmp(token, "bye") == 0) {
            printf("Bye!\n");
            break;
        }

        Node *parsed_exp = parse_expression();
        if (parsed_exp) {
            Node *result = eval(parsed_exp, &env);
            if (result && result->type != ERROR) {
                 print_node(result);
                 printf("\n");
            } else if (result) {
                print_node(result);
                printf("\n");
            }
        } else {
            printf("Parse error.\n");
        }
    }
    return 0;
}