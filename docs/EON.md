# Easy Object Notation

Neothyne defines rendering techniques using a YAML-like language called
EON.

The grammar is:

```
whitespace = " "
newline = "\n"
letter = ?[a-Z]?
digit = ?[0-9]?
comment = { whitespace } , "#" , { ?*? } , newline ;
identifier = ( letter | "_" ) , { letter | digit | "_" } ;
terminal = '"' , character , { character } , '"' ;
section = identifier , ":" , { whitespace } , [ newline ] , { value } ;
value = identifier , newline | terminal , newline , ( { ( identifier | terminal ) , "," } , newline ) ;
document = { { whitespace } , section , { whitespace } , { value } , . ;
```

Where:
- ?[a-Z]? *means any character from a to Z (note the case)*
- ?[0-9]? *means any digit from 0 to 9*
- ?*? *means any printable character*

An example of EON

```
# This is a comment
section1:
  # This is another comment
  field1: value2
  # Multiple values
  field2: value1, value2
  # Terminals allow spaces in identifiers
  "spaces in section": "spaces in value"
  # Multiple values as new line list
  field3:
    value1
    value2
    value3
  # Mix them even
  field4: a, b
    c, d, e
    f, g, h

section2:
  field: "some spaces in this value"
  "some spaces in this field": value
```

Like YAML - the indentation does matter, failing to maintain the correct
nesting level will cause parse errors.

The stylistic choice in Neothyne is to use two spaces for EON. However
this is purely a choice, any number of spaces can be used as the
indentation width. Tabulators are supported as well.

## Render Techniques

The technique parser supports the following fields and is invoked by
creating a `method` section

  - "name" *a name specifying the technique*
  - "vertex" *the vertex shader file*
  - "fragment" *the fragment shader file*
  - "defines" *a list of defines to splice into the vertex and fragment source*
  - "uniforms" *a list of unfiorms as fields, with their values set to the uniform type*
  - "attributes" *a list of program attributes*
  - "fragdata" *a list of fragment data*

The following are legal types for uniforms
  - "int2"
  - "vec2"
  - "vec3"
  - "vec4"
  - "mat4"
  - "mat3x4[]"
  - "float"

Uniform names can contain a prefix of the form: `[value]` where *value*
is a decimal literal representing an array length. Only one dimensional
arrays are supported.

Wrapping this all together; here is an example of a render technique
defined by EON:

```
method:
  name: example
  vertex: example.vs
  fragment: example.fs
  defines:
    MACRO_NO_VALUE
    MACRO_WITH_A: VALUE
  attributes: position, coordinate
  uniforms:
    wvp: mat4
    # Engine interprets this as instances[0] .. instances[39] for uniforms
    # i.: uniform arrays are possible
    instances[40]: mat4
```
