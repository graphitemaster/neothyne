# Developers
Documentation about engine components can be found in here

## Organization
The source code is organized into namespaces and a file name convention.
For example, anything prefixed with `u_` belongs to the *utility* namespace,
which is also named `u`. The following namespaces and file prefixes exist:

| Prefix | Namespace | Description |
|--------|-----------|-------------|
| u_     | u         | utility     |
| m_     | m         | math        |
| r_     | r         | renderer    |
| c_     | c         | console     |

## Utility library
Neothyne doesn't use the C++ standard library, instead it rolls it's own
implementation of common containers. They're not meant to be compatible with the
standard in any way, even if they share a similar API. The following containers
exist.

* u::pair
* u::set
* u::map
* u::string
* u::vector
* u::buffer
* u::stack

As well as a plethora of type traits and algorithmic functions which can be
found in `u_traits.h` and `u_algorithm.h` respectively. Miscellaneous utilites can
be found in `u_misc.h`.

## Shaders

Shaders in Neothyne are written in GLSL with a bit of syntax sugar.

Neothyne will emit a prelude to each shader containing the version of GLSL to
utilize as well as things like `HAS_TEXTURE_RECTANGLE` if the GPU supports that
feature. You can find a list of the macros it emits by searching for
`method::define` in the code-base.

Neothyne supports the `#include` directive in GLSL; similarly, Neothyne will emit
uniform guards to prevent declaring uniforms more than once.

The following code:
```
    uniform vec2 foo;
    uniform vec4 bar;
```

Will be converted into:
```
    #ifndef uniform_foo
    uniform vec2 foo;
    #define uniform_foo
    #define line 1
    #endif
    #ifndef uniform_bar
    uniform vec4 bar;
    #define line 2
    #endif
```
This way you can have uniforms declared in headers which can be included without
causing conflicts.
