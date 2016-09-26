# Neothyne scripting language

## Types and Values
Neo is a *dynamically typed* language that borrows heavily from Javascript and Lua.

All values in Neo are *first-class value*. This means that all values can be stored in variables,
passed as arguments to other functions and returned as results.

There are nine types in Neo: `null`, `bool`, `int`, `float`, `array`, `object`, `string`, `function`, `method`.
The type `null` has a single value, `null`, who's only propery is to be different from any other value. It's useful to represent the absense of a value.
The type `bool` has two values, `true` and `false`. Both `null` and `false` make a condition false; any other value makes it true.
The type `int` represents signed integer numbers.
The type `float` represents real (floating-point) numbers.
The type `array` represents a *hetrogeneous* array.
The type `object` represents a *hetrogeneous* associative array. Objects are the sole data-structure
mechanism in Neo; they can be used to represent symbol tables, sets, records, graphs, trees, etc. Neo
uses the field name as an index. This is provided by `a.name` being syntatic sugar for `a["name"]`.
The type `string` represents immutable sequence of bytes.
The type `function` represents a function.
The type `method` represents a special function with an encapsulating `this` local parameter for
objects similar to C++ classes.

## Garbage Collection
Neo performs automatic memory management. This means that you don't have to worry about allocating
memory for new objects or freeing it when the objects are no longer needed. Neo acomplishes this by
running a *garbage collector* to collect all *dead objects* (objects that are no longer accessible from Neo). **All memory in Neo is subject to automatic memory management**

# Language
Neo is a free-form language. It ignores spaces (including new lines) and comments between tokens, except
when used as delimiters between identifiers and keywords.

## Comments
Comments in Neo can be represented in two ways
```
// this is a single line comment
/* this is a multi
   line comment */
```
**Comments cannot nest**

### Identifiers
Identifiers in Neo can be any string of characters, digits and underscores, not beginning with a digit
and not being a reserved word. Identifiers are used to name variables and fields.

The following keywords are reserved and cannot be used as identifiers
```
let new if else while fn method
```

Neo is *case-sensitive* all of the following are unique identifiers:
```
foo FOO Foo
```

The following is a list of recognized tokens
```
+ - * / // /* */ ! == != = < !< > !> <= !<= >= !>= ( ) { } [ ] , ; "
```

Literal strings are deliminted by matching double quotes
```
"like this"
```

## Variables
Variables are places that store values. There are two kinds of variables in Neo; local variables and
object fields. A single name denotes a local variable or a function's formal parameter, which is a
particular kind of local variable.

Local variables are *lexically scoped*: can be freely accesses by functions or methods defined inside
their scope.

Before the first assignment to a variable, its value is `null`. Unlike Lua, variables cannot be assigned to unless they've been created. Creating variables is done with the `let` keyword.
```
let a = 5;
let b; // defaults to null
let c = 5, d, e = "hi"; // also allowed
```
Square brackes can be used to index an array or an object.
```
array[index]; // array indexing
object["field"]; // object indexing
```

The syntax `variable.name` is just syntatic sugar for `variable["name"]`

## Statements
Neo supports a conventional set of statements, similar to those in C. This includes assignments,
control structures, function calls and variable declarations.

Function calls and assignments can start with an open parenthesis. In Lua this leads to a grammatical
ambiguity
```
a = b + c
(print or io.print)('done')
```
The grammar in Lua sees this as
```
a = b + c(print or io.write)('done')
```

These sorts of problems do not happen in Neo which requires all statements be terminated
with a semicolon.
```
statement; // semicolon keeps grammatical ambiguities from occuring
```

### Blocks
A block is a list of statements, which are executed sequentially. A block is created
for every function and path in a control statement . Unlike Javascript and Lua, `{` and `}`
can be used to introduce a new block but not a new scope since
**blocks do not create new scopes,** variable declarations do however, lexical scopes
reset the "active scope" at the end. This, unlike Lua or Javascript allows us to close the
scope immediately after the variable declaration, allowing later optimization.

### Assignment
The assignment statement first evaluates all its expressions and only then the assignments
are performed.

### Control Structures
The control structures `if` and `while` and `return` have the usual meaning and familiar syntax.
The condition expression of a control structure can return any value. Both `false` and `null` are
considered false. All values different from `false` and `null` are considered `true`. This means
the number 0 and empty string are also `true`.

The `return` statement can be written anywhere in a block. In contrast to Lua which only allows
it as the last statement of a block.

### Arithmetic operators
Neo supports the following arithmetic operators:
* `+` addition
* `-` subtraction
*  `*` multiplication
*  `/` division

### Coercions and Conversions
Neo provides some automatic conversions between some types and representations ar runtime.

Arithmetic operations applied to mixed types (integers and floats) convert the integer to a float; this is
the usual conversion rule.

Neo implements the Javascript prototype chain technique, so objects can subclass other objects
like primitives. When the runtime encounters these it will lookup the base representation in
the prototype chain.

For example you can do the following:
```
let a = new 5 { a = 10 }; // a subclasses the int type (5 is int)
print(a); // will search the prototype chain and find 5 for a and print 5
```

### Relational operators
* `==` equality
* `!=` inequality
* `<` less than
* `!<` not less than
* `>` greater than
* `!>` not greater than
* `<=` less than or equal to
* `!<=` not less than or equal to
* `>=` greater than or equal to
* `!>=` not greater than or equal to

These operators always result in `true` or `false`.

### Concatenation
The arithmetic `+` operator is used for string concatenation.

### Object literals
Object literals are expressions that create objects. Evertime an object literal is evaluated, a new
object is created. An object literal can be used to create an empty object or to create an object
and fill some its fields.
```
// as an example
let empty = {};
let object = { a = value, b = "nope", c = fn() { /*...*/ } };
```

### Array literals
Array literals are expressions that create arrays. Everytime an array literal is evaluated, a new
array is created. An array literal can be used to create an empty array or to create an array and
give it contents.
```
// as an example
let empty = [];
let array = [ 1, 2, 3, 4 ];
```

### Function and Method Definitions
There is only one true function definition
```
let name = fn(/*arguments*/) { /*body*/ };
```
However the following is also allowed and is sugar for the above
```
name fn(/*arguments*/) { /*body*/ }
```

Methods are also just sugar too
```
obj.foo = method() { /*body*/ }
// translates to
obj.foo = fn(this, /*arguments*/) { /* body */ }
```

### Function and Method calls
Function calls are farily straight forward and work similarly to Javascript. There is no
omitting or silently filling of a function's formal prameters like in Lua.

Calls to methods implicitly pass the object as the first argument in the function.

### Visibility
Neo is a lexically scoped language. The scope of a local variable begins at the first statement
after it's declaration and lasts until lexically we're no longer in that scope.