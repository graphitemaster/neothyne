# Header files
All header files should have a header guard in the form of
    #ifndef FILENAME_HDR
    #define FILENAME_HDR
    ...
    #endif

The use of #pragma once is forbidden.

A header file should forward declare any class types it may use and try to include
the least amount of external references as possible.

Every header file should, with just a rename of the file extension, compile as
is without warning or error.

Header files shouldn't contain inline functions unless they're trivial in nature
and can be expressed in *five-or-fewer* lines.

Templated classes and class functions should favor the *implement-after-define*
approach opposed to inline style. An example is provided below.

    // For trivial classes this is fine
    template <typename T>
    struct foo {
        void bar() {
            // ...
        }
    };

    // This is generally preferred
    template <typename T>
    struct foo {
        void bar();
    };

    template <typename T>
    void foo<T>::bar() {
        // ...
    }

The reason for this is that more often than not, when working with a templated
class, the class itself is a container. It's much easier to browse the top of
the header file for a quick synopsis of its functions than it is to look at
implementation details.

# Namespaces
The use of namespaces is encouraged provided that the hierarchy does not exceed
two nesting levels.

Typically most ideas can be categorized into one very vague term, that vague term
or word would be the name of the namespace; for instance, utilities should go in
the namespace, appropriately named "utility".

Namespace names should be short, concise, and easy to type repeatedly as the
use of "using namespace" is discouraged.

Don't indent contents of the namespace the same way other scopes are indented,
everything should stay on the first column in a namespace; however a nested
namespace's contents should be indented. An example is provided below.

    namespace foo {

    void foo(); // Don't indent the contents

    // Nested namespaces are never indented
    namespace detail {
        void bar(); // However their contents are
    }

    }

When working with implementation details that cannot be hid it's often common to
utilize a namespace, a common theme is to name such namespace "detail". This is
fine, other names for such namespace are forbidden.

Unnamed and inline namespaces are forbidden.

# Classes
Avoid doing complex initialization in constructors. Constructors can never, and
should never fail as there is no way to indicate errors, short of using exceptions
(which are forbidden.) If the object requires non-trivial initialization, consider
using an init() function.

If the class defines member variables, every variable must be explicitly initialized
in all class constructors initializers-lists. If the member variable cannot be
initialized in the initializer-list, then it should be initialized in the body of
the constructor.

The use of the 'explicit' keyword for class constructors is encouraged whenever
appropriate.

Use delegating and inheriting constructors when they reduce code duplication.

Never use the 'class' keyword, always use 'struct'. The former is private by
default which is wrong as private members should always be at the bottom of
the class definition.

The use of private, public and protected is encouraged but not required, if how
ever it is used, accessor functions should not be prefixed with "get", but mutator
functions should. An example is provided below.

    struct foo {
        void setFoo(int foo) {
            m_foo = foo;
        }

        int foo() const {
            return m_foo;
        }
    private:
        int m_foo;
    };

Do note that the use of accessor and mutator functions is only encouraged if the
underlying datum they access and mutate is depended upon by other components of
the class. If the datum is not depended upon internally, then the datum should
be made public and accessor and mutator functions are discouraged.

The use of inheritance is allowed provided that the inheritance is public. Don't
overuse inheritance, composition is often more appropriate. Try to restrict the
use of inheritance to the "is-a" case: 'bar' subclasses 'foo' if it can be
reasonably said that 'bar' "is-a kind of" 'foo'.

Multiple inheritance is forbidden.

The use of 'virtual' is forbidden.

Operator overloading is discouraged for everything except container types that
are trying to act as if they were built-in types and maths types like vectors,
matrices, quaternions, etc. The use of user-defined literals is forbidden.

All classes should have a swap function that does a shallow-swap.

The use of friend classes and functions is allowed within reason. Friends should
only be used if the encapsulation boundary of the class should be extended to the
friend, if the use of 'friend' is to simply break the encapsulation boundary of
the class, then just make the members public.

# References and pointers
All parameters passed by reference must be labeled const.

All parameters passed by pointer and reference must satisfy the restrict argument
in that the reference or pointer is not aliased in the local context. All functions
must satisfy this constraint, thus functions like 'memmove' are not possible and
are forbidden. This doesn't mean you shouldn't use memmove, it just means you're
forbid from writing a function like memmove.

Use rvalue references when appropriate. The use of 'forward' is encouraged.

Using raw pointers for raw memory is discouraged, use vector<T>
or unique_ptr<T[]> instead.

The use of raw pointers is encouraged in situations where references cannot be
used.

# Const and constexpr
Use const and constexpr where ever possible.

Constants should always be prefixed with 'k'. An example is provided below.

    constexpr size_t kSomething = 1024;

Use const cv-qualification where ever possible except when the function clearly
breaks cv-q indirectly, the use of 'const' is a good indicator of if the function
mutates what is passed. If it's marked const but still mutates indirectly, then
the use of the qualifier is inappropriate.

# Integer types
When working with sizes always use size_t. Lots of functions that utilize sizes
depend on `size_t` as part of their interface contract. Blindly breaking this with
the usage of integers is forbidden.

The use of short, long and long long is forbidden.

For situations when the exact size of types is important, make appropriate
use of `<stdint.h>`

# Typedefs and using
Typedefs and using are discouraged, but not forbidden. Use typedef and using
very carefully as they can hide important type information.

# Functions
Function overloading is discouraged if the overload only exists to satisfy a
different type. Use a templated function instead to suppress implicit type
conversion rules of the language.

For functions which need to indicate failure or error state, use optional<T> instead
of indicating error through other means.

The use of default arguments is discouraged if the function's address may be
taken in the context of a function pointer.

Functions which do not return shall be marked `[[noreturn]]`.

Functions taking arrays should do so in the form of a 'reference to array',
opposed to pointer-to-array with optional size argument. This gives the compiler
context for optimization and can help prevent bugs such as overrunning the
array. A few examples are provided below.

    // This is discouraged
    void foo(int *stuff, size_t nstuff) {
    }

    // If you know the size, type it in place as such
    void foo(int (&stuff)[5]) {
    }

    // Otherwise use a template
    template <size_t E>
    void foo(int (&stuff)[E]) {
    }

# Preprocessor
The preprocessor cannot be entirely avoided. The use of macros is however highly
cautious as most things can often be represented with inline functions, enums
and constant variables.

# Null considered harmful
The use of anything but nullptr for pointers is forbidden.

For variable argument functions always explicitly use `(void*)0` as a `NULL`
variable argument instead of `nullptr` or `NULL`.

# Sizeof
Prefer `sizeof(varname)` to `sizeof(type)`.

# Auto
The use of `auto` is only encouraged if the typename is long or ends up being written
more than once. Otherwise the use of it is discouraged as it can break readability.
Some examples are provided below.

    for (auto &it : foo) // Fine as the alternative is too long

    auto foo = (int)something; // Explicit cast shows the type so writing it again is redundant

    auto a = vec3(1, 2, 3); // The type is obvious by the right hand side

    auto b = a + a; // This is discouraged, no type information is present

# Brace initializer list
The use of brace initializer lists is encouraged.

Assigning a brace-initalized list to an auto local variable is forbidden.

# Lambdas
Use lambdas expressions where appropriate.

Never use default lambda captures; write all captures explicitly.

# Memory
The use of new and delete is discouraged except in the context of `unique_ptr`.

When implementing container types, it's encouraged you utilize raw memory allocation
mechanisms like malloc/realloc/free and use placement new to initialize objects.
The reason for this is new/delete leave very little room for optimization and
have an unintended overhead. Things like vector<T> can be made much more efficient
if it utilizes realloc to resize the memory in favor of throwing away its
internal memory every time a resize is needed. To correctly do this, the use of
`is_pod<T>` is required.

Use `unique_ptr<T>` whenever possible over `T*`.

When not implementing container types and raw memory is needed, use
`vector<unsigned char>`.

# Exceptions
The use of exceptions is forbidden.

# Runtime type information
Any operation that depends on runtime type information is forbidden. This
includes, but is not limited to: `dynamic_cast`, virtual functions, `typeid()`.

Decision trees based on type are a strong indication that the code is on the wrong
track. You can often achieve the same effect with a tagged union.

# Casting
The use of C++ style casts is forbidden. The one notable exception to this
is when implementing forward or move which requires the use of `static_cast`.
In all other situations, use C style casts.

# Shifting
Avoid the use of undefined shifts like signed left shifts. Neothyne has a
utility that allows safely doing this called `u::sls`.

# Vectors
The use of vector is encouraged for all tasks that are not key, or key-value
associative. Operations that typically make sense as a linked-list for instance
would also fall under this category.

When appending to a vector of which you know the length of, it's always wise to
reserve or resize the vector to accommodate the append.

# Linked lists
Linked lists are forbidden for any task.

# Map/Set
Only use `unordered_map` and `unordered_set`. The use of map and set is otherwise
forbidden.

# Strings
The use of a managed string type is encouraged if and only if all paths leading
to it are mutable. If even a single path leading to it is immutable, then the use
of a managed string type is discouraged.

Try to avoid using expensive operations like `pop_front()`.

When appending content to an existing string of which the length is known, always
favor the methods which allow you to specify the length as an optional argument.
If no such means are possible, then utilize the reserve function of the string
object.

# Streams
Streams are forbidden. The use of printf and family is already type-checked by
most major compilers and is much easier to utilize. You can always use variadic
templates to create a more feature-rich printf.

# Printf and family
Using the correct format specifier for types is something that still eludes people.

When printing a `size_t` always use *%zu* as the format specifier.

When printing a pointer always cast it to `(void*)` and always use *%p*. Never use
*%x* or *%lx*.

When printing `int64_t` use *%"PRId64"*.

When printing `uint64_t` use *%"PRIu64"* or *%"PRIx64"*

When printing `ptrdiff_t` use *%"PRIds"*

# Boost
The use of the boost libraries is forbidden.

# Naming
Function names, variable names, and filenames should be descriptive but should
still be terse. Abbreviations are allowed as long as they're obvious.

Members of classes are always prefixed with 'm_', so long as they're private or
protected, public members have no prefixes.

Globals are always prefixed with 'g'.

Constants are always prefixed with 'k'.

Macros are all upper-case, macro parameters are too.

All naming makes used of 'camelCase'. This holds true for prefixes as well,
for instance "gThatThing", not "gthatThing". This is not true for members however
where the correct naming would be "m_thatThing" and not "m_ThatThing".

A small code example is provided below to show the naming

    #define ACK_SIGNATURE(X) ((X)+1)

    static constexpr kMaxConnections = 5;

    enum {
        kConnectLocal,
        kConnectRemote
    };

    struct connection {
        connection(int fd, int type)
            : m_alive(false)
            , fd(fd)
            , type(type)
        {
        }

        int fd;
        int type;

        void acknowledge() {
            sendAck(fd, ACK_SIGNATURE(fd));
            while (poolAck())
                ;
            m_alive = getAck();
        }

    private:
        bool m_alive;
    };

    static connection gConnections[kMaxConnections];
    static size_t gConnectionCount = 0;

    void acceptConnection(int fd) {
        if (gConnectionCount >= kMaxConnections)
            return;
        gConnections[gConnectionCount++] = {
            fd,
            isConnectionLocal(fd) ? kConnectLocal : kConnectRemote
        };
        gConnections[gConnection].acknowledge();
    }

There are exceptions to these naming rules. When naming something analogous to
an existing C or C++ entity then you can follow the same naming convention scheme.
For instance if implementing a standard library like entity, then the naming
scheme should be adopted, const_iterator over constIterator or hash_map opposed
to hashMap.

Filenames should be all lowercase and can include underscores. All other characters
outside "a-Z", "0-9" and "_" is forbidden in a file name. C++ files should end
with the extension ".cpp", while header files should end with ".h". Don't use
common file names.

# Comments
Comments are vital to keeping code readable. Code should be self-documenting but
when it isn't comments should never describe the code itself but rather what
the code is attempting to do.

# Indentation
Never use tabulators, always use spaces. When indenting, you indent 4 spaces
at a time.

Return type is always on the same line as the function name.

Initializer lists for classes always look like this
    foo::foo()
        : first(a)
        , second(c)
        , third(d)
    {
    }

Everything uses the one-true-brace-style

    void foo() {
        // ...
    }

    if (this) {
        // ...
    } else {
        // ...
    }

Always a space after if, while and for before the parenthesis

    if (foo)
    for (...)
    while (...)

Always use a space after 'template'
    template <typename T>

No spaces for function calls, except after the separators in function arguments

    foo(1, 2, 3);

Ternary operator usage should also be indented like if/else if they span multiple
lines

    a = somethingVeryComplex(&b, &c) && somethingVeryLongAsWell(&d, &e)
            ? onTrueSomethingHere(spanningMultipleLinesToo(123, 3.14))
                ? ohLookAnotherOne()
                : onFalseAnotherOne()
            : finaFalseThing();

Switch statements should never use braces for blocks. If you need locals for
a case, lift them outside the switch statement.

The reference '&' and '*' indicators always go on the name, never the type.

    int a = 0;
    int *b = &a;
    int &c = *b;

No spaces around period or arrow.

Don't needlessly surround expressions with parentheses. Parentheses should only
be used when the default precedence of operators is incorrect for what you're
trying to achieve, or when the compiler produces a warning diagnostic that can
only be suppressed with an additional set of parentheses.

Preprocessor directives should be indented with three spaces after the hash

    #ifdef foo
    #   ifdef bar
    #       ifdef baz
    #           define one 1
    #       else
    #           define one 2
    #       endif
    #   else
    #       define one 3
    #   endif
    #endif

Spacing in for loops is crucial. If something of the for loop is to be omitted
a whitespace should be inserted to indicate what was omitted

    for (; i < 10; ) {
        // Correct
    }

    for (; i < 10;) {
        // Incorrect
    }

When omitting the block of a for or while loop, use a newline followed by
an appropriately indented semicolon to indicate that the scope is meant to
be omitted.

    // Correct
    while (something())
        ;

    // Incorrect
    while (something());


Trailing whitespace is forbidden

A single space character following another character is not allowed unless it's
a span of space characters forming the indentation or to align '\' in a multi-line
macro. Otherwise, whitespace used for alignment is forbidden.

    // Don't write this
    hello = 1;
    a     = 2; // Aligned '='

    // Do this instead
    hello = 1;
    a = 2;

    // This however is an appropriate use of space characters
    #define foo     \
        do {        \
            task(); \
        } while (0);
