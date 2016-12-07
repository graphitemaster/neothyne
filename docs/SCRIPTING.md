# Neothyne scripting language

## Types and Values
Neo is a *dynamically typed* language that borrows heavily from
Javascript, C++, Lua, Ruby, C#, Java (In order of inspiration.)

All values in Neo are *first-class values*. This means that all values can be
stored in variables,
passed as arguments to other functions and returned as results.

There are nine types in Neo: `null`, `bool`, `int`, `float`, `array`, `object`,
`string`, `function`, `method`.

The type `null` has a single value, `null`, who's only propery is to be
different from any other value. It's useful when representing the absence of a
value and works exactly like Lua's `nil` type.

The type `bool` has two values, `true` and `false`. Both `null` and `false` make
a condition false; any other value makes it true. This means values like `0`
and empty string `""` also evaluate true.

The type `int` represents a *signed* integer numbers in the range of
`[-2,147,483,648, 2,147,483,647]`. Numbers outside that range *cannot* be
represented.

The type `float` represents a real (floating-point) number in the range of
`[1.175494e-38, 3.402823e+38]`.

The type `array` represents an *heterogeneous* array. Heterogeneous means it
can store anything, even other arrays.

The type `object` represents an *heterogeneous* associative array. Objects are
the sole data-structure mechanism in Neo; they can be used to represent symbol
tables, sets, records, graphs, trees, etc. Neo uses the field name as an index.
This is provided by `a.name` being syntactic sugar for `a["name"]`.

The type `string` represents an immutable sequence of bytes. Strings in Neo
are 8-bit clean.

The type `function` represents a function.

The type `method` represents a special function with an encapsulating `this`
local parameter for objects similar to C++ classes.

## Garbage Collection
Neo performs automatic memory management. This means that you don't have to
worry about allocating memory for new objects or freeing it when the objects are
no longer needed. Neo accomplishes this by running a *garbage collector* to
collect all *dead objects* (objects that are no longer accessible from Neo).
**All memory in Neo is subject to automatic memory management**

# Language
Neo is a free-form language. It ignores spaces (including new lines) and
comments between tokens, except when used as delimiters between identifiers and
keywords.

## Comments
Comments in Neo can be represented in two ways
```
// this is a single line comment
/* this is a multi
   line comment */
```
**Comments cannot nest**

### Identifiers
Identifiers in Neo can be any string of characters, digits and underscores, not
beginning with a digit and not being a reserved word. Identifiers are used to
name variables and fields.

The following keywords are reserved and cannot be used as identifiers
```
let new if else while for fn method
```

Neo is *case-sensitive*, all of the following are unique identifiers:
```
foo FOO Foo
```

The following is a list of recognized tokens:
```
+ - * / += -= *= /= // /* */ ! == != = < !< > !> <= !<= >= !>= ( ) { } [ ] , ; "
```

Literal strings are delimited by matching double quotes
```
"like this"
```

## Variables
Variables are places that store values. There are two kinds of variables in Neo;
local variables and object fields. A single name denotes a local variable or a
function's formal parameter, which is a particular kind of local variable.

Local variables are *lexically scoped*, which means they can be freely accessed
by functions or methods defined inside their scope.

Before the first assignment to a variable, its value is `null`. Unlike Lua,
variables cannot be assigned to unless they've been created.
Creating variables is done with the `let` keyword.
```
let a = 5;
let b; // defaults to null
let c = 5, d, e = "hi"; // also allowed
```
Square brackets can be used to index an array or an object.
```
array[index]; // array indexing
object["field"]; // object indexing
```
The dot `.` can be used to index an object
```
object.field
```
The syntax `variable.name` is just syntactic sugar for `variable["name"]`

## Statements
Neo supports a conventional set of statements, similar to those in C.
This includes assignments, control structures, function calls and variable
declarations.

Function calls and assignments can start with an open parenthesis. In Lua this
leads to a grammatical ambiguity
```
a = b + c
(print or io.print)('done')
```
The grammar in Lua sees this as
```
a = b + c(print or io.write)('done')
```

These sorts of problems do not happen in Neo which requires all statements to
be terminated with a semicolon.
```
statement; // semicolon keeps grammatical ambiguities from occuring
```

### Blocks
A block is a list of statements, which are executed sequentially.
A block is created for every function and path in a control statement. Unlike
Javascript and Lua, `{` and `}` can be used to introduce a new block but not a
new scope since **blocks do not create new scopes,** variable declarations do
however, lexical scopesreset the "active scope" at the end. This, unlike Lua or
Javascript allows us to close the scope immediately after the variable
declaration, allowing later optimization.

### Assignment
Assignment is done with the `=` operator and there also exists the following
compound assignment operators.
```
a += b; // sugar for: a = a + b
a -= b; // sugar for: a = a - b
a /= b; // sugar for: a = a / b
a *= b; // sugar for: a = a * b
```

The assignment statement first evaluates all its expressions and only then the
assignment is performed. All expressions are evaluated left to right. This is
in contrast to Javascript and many other languages that leave this up to
*sequence points* and in some cases undefined.


### Classes
Neo supports classes through the Javascript method of prototype-chains.
The `new` keyword can be used to construct a new instance of an Object and
also subclass an existing object.
```
let a = { a = 1 };
let b = new a { b = 2 }; // subclassing: b.a = 1 and b.b = 2
let c = new b;           // constructs a new instance of b
b.b = 10;                // does not change c.b since c is a new instance of b
```

This may appear strange to some programmers as we're subclassing on an object
and not a type. In Neo classes are always object references opposed to the more
traditional value-type approach in languages like C++.

#### Inheritence
Neo language supports single-inheritence multiple-interface object oriented
programming, much like Java and C#.

The decision to not support multiple-inheritence has to deal with Neo's strong
interpretation of Liskov's Principle. The Liskov Principle (LSP), in summary,
states that a class inheritence is empirically a strong "is-a" relationship.
Which means any object of a subclass is-a object of the parent class.
For instance any *Cat* is-a *Animal*. There is a major difference to be made
between "is-a" and "can-be-with-complex-transformation". What this means is that
any object of a subclass is also immediately (and indefinitely) an object of
all superclasses. In Neo there is a root class called Object which forms the
root of the object hierarchy. What this means is a reference to the "vtable"
(which Neo doesn't really have in the traditional sense) has to be in Object
itself, since you couldn't for instance have it at the end of the class
otherwise adding new members would move the reference to the "vtable" around.

This however is not the final nail in the coffin. LSP also states that anything
which is added to a superclass in the creation of a subclass must be "beneath"
all the data which describes the superclass. This idea does not permit any data
that belongs to the superclass to be moved around, shifted, etc. This also
includes the data a "vtable" traditionally encodes. If the object has a
reference to it than it must be beneath as well.

So basically new members go underneath the superclass's members and new
functions underneath the old class's functions and so on. This technique does
not permit a clean and straight forward implementation of multiple-inheritence
though. So why would we want it at all then? It's simple and efficent because
no data moves around which means references to a superclass from a subclass are
always static after the fact. For instance we can inline the access during
execution. This also means that functions and members cannot be removed.

Neo language is very dynamic so it isn't difficult to actually emulate
interfaces with classes itself. This idea was derived from the observation that
interfaces are essentially objects and that the LSP rules work for them too.
For instance if you consider that all interface references are-a reference to
their first parent interface and that an interface can only add functions, not
data. Then they operate exactly the same way.

This means all that is required of an interface function is to call them with
object references.

Similarly, techniques like multiple dispatch can be done with multiple levels
of single dispatch in Neo by creating the appropriate interface object
and subclassing it for your classes that implement that interface.

### Control Structures
The control structures `if` and `while` and `return` have the usual meaning and
familiar syntax. The condition expression of a control structure can return any
value. Both `false` and `null` are considered false. All values different from
`false` and `null` are considered true. This means the number `0` and empty
string `""` are also `true`.

The `return` statement can be written anywhere in a block. In contrast to Lua
which only allows it as the last statement of a block.

### Arithmetic operators
Neo supports the following arithmetic operators:
* `+` addition
* `-` subtraction
*  `*` multiplication
*  `/` division

### Coercions and Conversions
Neo provides some automatic conversions between some types and representations
at runtime.

Arithmetic operations applied to mixed types (integers and floats) convert the
integer to a float; this is the usual conversion rule.

Neo implements the Javascript prototype chain technique, so objects can subclass
other objects like primitives. When the runtime encounters these it will lookup
the base representation in the prototype chain.

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

Note: The *not* variants are treated as a negation on the typical relational
operator.
```
a != b   // sugar for !(a == b)
a !< b   // sugar for !(a < b)
a !> b   // sugar for !(a > b)
a !<= b  // sugar for !(a <= b)
a !>= b  // sugar for !(a >= b)
```
These operators always result in `true` or `false`.

### Concatenation
The arithmetic `+` operator is used for string concatenation.

## Integer and Floating literals
When working with numeric values, literals that do not have a decimal point
somewhere are treated as `int`, otherwise they are treated as `float`. Numeric
literals may also begin with `0x` and contain `[0-9a-Z]` to represent a
*hexadecimal* (base 16) value.
```
1; // int
1.; // float
.1; // float
0x12; // int
```

### Object literals
Object literals are expressions that create objects. Everytime an object literal
is evaluated, a new object is created. An object literal can be used to create
an empty object or to create an object and fill some of its fields.
```
// as an example
let empty = {};
let object = { a = value, b = "nope", c = fn() { /*...*/ } };
```

### Array literals
Array literals are expressions that create arrays. Everytime an array literal
is evaluated, a new array is created. An array literal can be used to create an
empty array or to create an array and give it contents.
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
Function calls are fairly straightforward and work similarly to Javascript.
Unlike Lua, Neo **does not** attempt to make function calls with incorrect
*arity*; eg. if the function is declared to take five parameters then the
function **must** be called with five arguments.
```
fn foo(a, b, c) { /*...*/ }
foo(1, 2., "hi"); // allowed
foo(); // not allowed
foo(1, 2, 3, 4); // not allowed
```

Calls to methods implicitly pass the object as the first argument in the
function.

### Visibility
Neo is a lexically scoped language. The scope of a local variable begins at
the first statement after its declaration and lasts until we're no longer in
that lexical scope. Because of lexical scoping rules, local variables can be
freely accessed by functions defined inside their scope. A local variable used
by an inner function is an *external local variable*, inside the inner function.
