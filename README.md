# listScript
Simple hybrid lisp interpreter 

# ListScript Interpreter

ListScript is a simple, Lisp-like interpreter written in C. It features a basic read-eval-print loop (REPL) and supports core programming concepts like variables, functions, and conditional logic.

## Getting Started

To compile and run the interpreter, simply use `gcc`.

### Compilation

```bash
gcc listscriptV3.c -o listscript
Running the REPL
Bash

./listscript
The REPL will greet you with "ListScript ready." and a -> prompt, where you can start writing code. Type bye to exit.

Language Features
1. Variables
You can define variables using the def keyword. def takes a symbol and a value and creates a new binding in the current environment.

-> def x 10
10
-> x
10
2. Core Primitives
ListScript includes several built-in operators that are part of its core library.

Arithmetic: +, -, *, /

Comparison: <, >, eq? (for equality)

List Manipulation: first (returns the first element of a list), rest (returns a new list with the first element removed), cons (adds a new element to the front of a list)

Booleans: true, false

3. Functions
Functions are defined using def with an args() list for parameters and a list() for the function body.

-> def add-one args(x) list(+ x 1)
-> add-one(5)
6
4. Lists
Lists are the primary data structure. They are created with the list() keyword. You can nest them to create complex data.

-> list(1 2 3)
list(1 2 3)
-> list(list(1 2) 3 4)
list(list(1 2) 3 4)
5. Conditionals
The if statement evaluates a condition and returns a value from one of two branches. It takes three arguments: a condition, a value to return if true, and a value to return if false. The condition must evaluate to a boolean value (true or false).

-> if list(eq? 5 5) 10 20
10
-> if list(> 10 100) list(1) list(0)
list(0)
6. Comments
You can add comments to your code using a semicolon ;. The interpreter will ignore anything from the semicolon to the end of the line.

-> def x 10 ; this defines a variable x with a value of 10
10


Input	Expected Output	What it Tests
def factorial args(x) list(if list(eq? x 1) 1 list(* x list(factorial list(- x 1))))	No output, just -> prompt	Recursive function definition. This defines a function that calls itself.
list(factorial 5)	120	Recursive function call. This is a classic test for recursion. 5! should evaluate to 120.
or you can do this 
factorial(5) returns 120 
    type bye to end REPL 

Future Extensions
Here are some ideas for how you could continue to improve ListScript:

Source File Execution: Implement a command-line flag, like --run <filename>, that would allow the interpreter to execute a script from a file instead of entering the REPL.

Multi-line expressions: The current read_line() function can only read one line at a time. This needs to be improved to handle expressions that span multiple lines, which is crucial for larger programs. The best way to do this is to keep reading lines until a balanced set of parentheses is found.

String Support: Add a new STRING data type to allow for text manipulation.

Metaprogramming: Add an eval primitive to allow the language to run code as data. This would enable the creation of powerful macros and custom control structures.
