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
    FUNCTION_CALL,
    STRING
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
        // For STRING nodes
        char *string;
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
    while (is_space(input_buffer[i]) || input_buffer[i] == ';') {
        if (input_buffer[i] == ';') {
            while (input_buffer[i] != '\n' && input_buffer[i] != '\0') {
                i++;
            }
        }
        if (input_buffer[i] == '\0') break;
        i++;
    }
    return input_buffer[i];
}

// Gets the next token from the buffer
int gettoken() {
    int index = 0;
    while (is_space(input_buffer[input_pos]) || input_buffer[input_pos] == ';') {
        if (input_buffer[input_pos] == ';') {
            while (input_buffer[input_pos] != '\n' && input_buffer[input_pos] != '\0') {
                input_pos++;
            }
        }
        input_pos++;
    }
    if (input_buffer[input_pos] == '\0' || input_buffer[input_pos] == EOF) return 0;
    
    if (input_buffer[input_pos] == '"') {
        input_pos++; // Consume the opening quote
        while (input_buffer[input_pos] != '"' && input_buffer[input_pos] != '\0' && index < SYMBOL_MAX - 1) {
            token[index++] = input_buffer[input_pos++];
        }
        if (input_buffer[input_pos] == '"') {
            input_pos++; // Consume the closing quote
        }
    } else if (is_parens(input_buffer[input_pos])) {
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

Node *make_string(char *str) {
    Node *node = make_node(STRING);
    node->value.string = strdup(str);
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
        Node *def_node = make_compound_node(DEF, 0);
        
        gettoken(); // Get the symbol to be defined
        def_node->value.compound.children = realloc(def_node->value.compound.children, (def_node->value.compound.child_count + 1) * sizeof(Node*));
        def_node->value.compound.children[def_node->value.compound.child_count++] = make_symbol(token);

        gettoken(); // Get the next token, which should be 'args'
        if (strcmp(token, "args") == 0) {
            def_node->value.compound.children = realloc(def_node->value.compound.children, (def_node->value.compound.child_count + 1) * sizeof(Node*));
            gettoken(); // consume '('
            Node *args_list = make_compound_node(ARGS, 0);
            while(gettoken() && token[0] != ')') {
                args_list->value.compound.children = realloc(args_list->value.compound.children, (args_list->value.compound.child_count + 1) * sizeof(Node*));
                args_list->value.compound.children[args_list->value.compound.child_count++] = make_symbol(token);
            }
            def_node->value.compound.children[def_node->value.compound.child_count++] = args_list;
            
            gettoken(); // get the first token of the body
            def_node->value.compound.children = realloc(def_node->value.compound.children, (def_node->value.compound.child_count + 1) * sizeof(Node*));
            def_node->value.compound.children[def_node->value.compound.child_count++] = parse_expression();

        } else {
            // It's a simple variable assignment
            Node *value_node = parse_expression();
            def_node->value.compound.children = realloc(def_node->value.compound.children, (def_node->value.compound.child_count + 1) * sizeof(Node*));
            def_node->value.compound.children[def_node->value.compound.child_count++] = value_node;
        }
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
    } else if (strcmp(token, "(") == 0) { // New check for opening parenthesis
        return parse_paren_content();
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
            } else if (input_buffer[input_pos - 1] == '"') {
                return make_string(token);
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
        case STRING:
        case PRIMITIVE_OP:
        case DATA:
            return expr;
        case LIST: {
            // Check if this list is a function call
            if (expr->value.compound.child_count > 0 && 
                (expr->value.compound.children[0]->type == SYMBOL || 
                 expr->value.compound.children[0]->type == PRIMITIVE_OP)) {
                // Construct a function call node and evaluate it
                Node *call_node = make_compound_node(FUNCTION_CALL, expr->value.compound.child_count);
                for (int i = 0; i < expr->value.compound.child_count; i++) {
                    call_node->value.compound.children[i] = expr->value.compound.children[i];
                }
                return eval(call_node, env);
            } else {
                // It's a data list, evaluate its children
                Node *new_list = make_compound_node(LIST, expr->value.compound.child_count);
                for (int i = 0; i < expr->value.compound.child_count; i++) {
                    new_list->value.compound.children[i] = eval(expr->value.compound.children[i], env);
                    if (new_list->value.compound.children[i]->type == ERROR) {
                        return new_list->value.compound.children[i];
                    }
                }
                return new_list;
            }
        }
        case FUNCTION_CALL: {
            if (expr->value.compound.child_count == 0) return make_compound_node(LIST, 0);
            
            Node *op = eval(expr->value.compound.children[0], env);
            if (op->type == ERROR) return op;

            // Collect and evaluate arguments
            Node **args = calloc(expr->value.compound.child_count - 1, sizeof(Node*));
            for (int i = 1; i < expr->value.compound.child_count; i++) {
                args[i-1] = eval(expr->value.compound.children[i], env);
                if (args[i-1]->type == ERROR) {
                    free(args);
                    return args[i-1];
                }
            }
            int arg_count = expr->value.compound.child_count - 1;

            if (op->type == PRIMITIVE_OP) {
                char *op_name = op->value.name;
                if (strcmp(op_name, "+") == 0 || strcmp(op_name, "-") == 0 || strcmp(op_name, "*") == 0 || strcmp(op_name, "/") == 0) {
                    if (arg_count != 2) return make_error("Arity mismatch: Expected 2 arguments for arithmetic operator");
                    if (args[0]->type != NUMBER || args[1]->type != NUMBER) return make_error("Type error: Arguments must be numbers");
                    long num1 = args[0]->value.number;
                    long num2 = args[1]->value.number;
                    if (strcmp(op_name, "+") == 0) return make_number(num1 + num2);
                    if (strcmp(op_name, "-") == 0) return make_number(num1 - num2);
                    if (strcmp(op_name, "*") == 0) return make_number(num1 * num2);
                    if (strcmp(op_name, "/") == 0) {
                        if (num2 == 0) return make_error("Division by zero");
                        return make_number(num1 / num2);
                    }
                } else if (strcmp(op_name, "<") == 0 || strcmp(op_name, ">") == 0 || strcmp(op_name, "eq?") == 0) {
                    if (arg_count != 2) return make_error("Arity mismatch: Expected 2 arguments for comparison operator");
                    if (args[0]->type != NUMBER || args[1]->type != NUMBER) return make_error("Type error: Arguments must be numbers");
                    long num1 = args[0]->value.number;
                    long num2 = args[1]->value.number;
                    if (strcmp(op_name, "<") == 0) return make_boolean(num1 < num2);
                    if (strcmp(op_name, ">") == 0) return make_boolean(num1 > num2);
                    if (strcmp(op_name, "eq?") == 0) return make_boolean(num1 == num2);
                } else if (strcmp(op_name, "first") == 0) {
                    if (arg_count != 1) return make_error("Arity mismatch: 'first' expects 1 argument");
                    if (args[0]->type != LIST) return make_error("Type error: 'first' expects a list");
                    if (args[0]->value.compound.child_count == 0) return make_error("Error: 'first' called on empty list");
                    return args[0]->value.compound.children[0];
                } else if (strcmp(op_name, "rest") == 0) {
                    if (arg_count != 1) return make_error("Arity mismatch: 'rest' expects 1 argument");
                    if (args[0]->type != LIST) return make_error("Type error: 'rest' expects a list");
                    if (args[0]->value.compound.child_count == 0) return make_error("Error: 'rest' called on empty list");
                    
                    Node *new_list = make_compound_node(LIST, args[0]->value.compound.child_count - 1);
                    for (int i = 1; i < args[0]->value.compound.child_count; i++) {
                        new_list->value.compound.children[i-1] = args[0]->value.compound.children[i];
                    }
                    return new_list;
                } else if (strcmp(op_name, "cons") == 0) {
                    if (arg_count != 2) return make_error("Arity mismatch: 'cons' expects 2 arguments");
                    Node *new_list = make_compound_node(LIST, 0);
                    new_list->value.compound.children = realloc(new_list->value.compound.children, (new_list->value.compound.child_count + 1) * sizeof(Node*));
                    new_list->value.compound.children[new_list->value.compound.child_count++] = args[0];
                    if (args[1]->type == LIST) {
                        new_list->value.compound.children = realloc(new_list->value.compound.children, (new_list->value.compound.child_count + args[1]->value.compound.child_count) * sizeof(Node*));
                        for (int i = 0; i < args[1]->value.compound.child_count; i++) {
                            new_list->value.compound.children[new_list->value.compound.child_count++] = args[1]->value.compound.children[i];
                        }
                    } else {
                        return make_error("Type error: 'cons' second argument must be a list");
                    }
                    return new_list;
                } else if (strcmp(op_name, "write") == 0) {
                    if (arg_count != 1) return make_error("Arity mismatch: 'write' expects 1 argument");
                    print_node(args[0]);
                    printf("\n");
                    return make_boolean(1); // Return a value to continue the REPL
                } else {
                    return make_error("Unknown primitive operator");
                }
            } else if (op->type == DEF) {
                // This is a user-defined function call
                Node *func_def_node = op;
                Node *args_node = func_def_node->value.compound.children[1];
                Node *body_node = func_def_node->value.compound.children[2];
                
                if (args_node->value.compound.child_count != arg_count) {
                    return make_error("Arity mismatch in user-defined function");
                }
                
                Env *local_env = *env;
                // Correct scoping: Add the function itself to the local environment for recursion
                local_env = define(local_env, func_def_node->value.compound.children[0]->value.name, func_def_node);

                for (int i = 0; i < args_node->value.compound.child_count; i++) {
                    Node *param_name = args_node->value.compound.children[i];
                    local_env = define(local_env, param_name->value.name, args[i]);
                }
                return eval(body_node, &local_env);
            }
            return make_error("Cannot apply a non-function or undefined operator");
        }
        case DEF: {
            char *name = expr->value.compound.children[0]->value.name;
            
            // This now handles both function and variable definitions
            if (expr->value.compound.child_count == 3) {
                 // It's a function definition, store the entire DEF node
                 *env = define(*env, name, expr);
                 return make_boolean(1); // Return a value to signify success
            } else {
                 // It's a simple variable assignment
                 Node *value = expr->value.compound.children[1];
                 Node *evaluated_value = eval(value, env);
                 *env = define(*env, name, evaluated_value);
                 return evaluated_value;
            }
        }
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
        case STRING:
            printf("\"%s\"", node->value.string);
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
    env = define(env, "first", make_primitive_op("first"));
    env = define(env, "rest", make_primitive_op("rest"));
    env = define(env, "cons", make_primitive_op("cons"));
    
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