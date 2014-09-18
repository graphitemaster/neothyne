#!/usr/bin/env python

import textwrap, sys, getopt

# Represents a GL function
class function:
    def __init__(self):
        self.formals = []
    type = ''       # Type of the function
    name = ''       # Name of the function
    formals = None  # List of tuples containing (type, name)

# Data types:
#   name    - OpenGL data type
#   format  - Format specifier to print `name' data type
#   promote - Data type it promotes to for va_arg
#   spec    - Unique specifier for it
types = [
    {'name': 'GLvoid',     'format': '',     'promote': '',             'spec': '0' },
    {'name': 'GLchar',     'format': '%c',   'promote': 'int',          'spec': '1' },
    {'name': 'GLenum',     'format': '0x%X', 'promote': 'unsigned int', 'spec': '2' },
    {'name': 'GLboolean',  'format': '%c',   'promote': 'int',          'spec': '3' },
    {'name': 'GLbitfield', 'format': '%u',   'promote': 'unsigned int', 'spec': '4' },
    {'name': 'GLbyte',     'format': '%x',   'promote': 'int',          'spec': '5' },
    {'name': 'GLshort',    'format': '%d',   'promote': 'int',          'spec': '6' },
    {'name': 'GLint',      'format': '%d',   'promote': 'int',          'spec': '7' },
    {'name': 'GLsizei',    'format': '%d',   'promote': 'int',          'spec': '8' },
    {'name': 'GLubyte',    'format': '%X',   'promote': 'unsigned int', 'spec': '9' },
    {'name': 'GLushort',   'format': '%u',   'promote': 'unsigned int', 'spec': 'a' },
    {'name': 'GLuint',     'format': '%u',   'promote': 'unsigned int', 'spec': 'b' },
    {'name': 'GLfloat',    'format': '%.2f', 'promote': 'double',       'spec': 'c' },
    {'name': 'GLclampf',   'format': '%f',   'promote': 'double',       'spec': 'd' },
    {'name': 'GLintptr',   'format': '%p',   'promote': 'intptr_t',     'spec': 'e' },
    {'name': 'GLsizeiptr', 'format': '%p',   'promote': 'intptr_t',     'spec': 'f' }
]

# Parses a file containing GL prototypes using the following grammar
#   <type> ":" <ident> "(" <formals> ")" <terminator>
# Where
#   <terminator>    ::= <EOL> | ";" <EOL>
#   <formals>       ::= <empty> | <formal> | <formal> "," <formal>
#   <formal>        ::= <type> ":" <ident>
#   <EOL>           ::= "\r\n" | "\n"
# Example
#   GLvoid: foo(GLint: x, GLfloat: y);
def readFunctions(functionFile):
    functions = []
    with open(functionFile) as data:
        for line in data.readlines():
            f = function()
            parse = line.split(':', 1)
            formals = parse[1].strip('\r\n;')
            # Get type and name
            f.type = parse[0].strip(' ')
            f.name = formals[:formals.find('(')].strip(' ')
            # Parse the formal list
            formals = formals[formals.find('(')+1:formals.find(')')].split(',')
            for formal in formals:
                split = formal.split(':')
                if len(split) == 2:
                    f.formals.append((split[0].strip(' '), split[1].strip(' ')))
            functions.append(f)
    return functions

# Prints the type and name of a GL function
def printTypeName(stream, function, name=True):
    if name:
        stream.write('%s %s' % (function.type, function.name))
    else:
        stream.write('%s' % (function.type))

# Prints the formals of a GL function
#   names   - toggle names
#   types   - toggle types
#   parens  - toggle parens
#   info    - toggle GL_INFO[P]
def printFormals(stream, function, names=True, types=True, parens=True, info=False):
    if parens:
        stream.write('(')
    for i, formal in enumerate(function.formals):
        if types:
            stream.write('%s' % (formal[0]))
        if names:
            stream.write((' %s' if types else '%s') % (formal[1]))
        if i != len(function.formals) - 1:
            stream.write(', ')
    if info:
        stream.write((' GL_INFOP' if len(function.formals) else 'GL_INFO'))
    if parens:
        stream.write(')')

# Prints the full prototype of a GL function
#   info      - toggle GL_INFO[P]
#   semicolon - toggle semicolon
def printPrototype(stream, function, info=False, semicolon=False):
    printTypeName(stream, function, True)
    printFormals(stream, function, True, True, True, info)
    stream.write(';\n' if semicolon else '\n')

# Print the GL_CHECK call with the right type spec string
def printCheck(stream, function):
    # Generate the spec string
    specs = ''
    for formal in function.formals:
        base = ''.join([i for i in formal[0].split() if i not in ['const']]).strip('*')
        count = formal[0].count('*')
        if count:
            specs += '*'
            if count != 1:
                base = 'GLvoid'
        specs += ''.join([x['spec'] for x in types if x['name'] == base])
    if len(function.formals):
        stream.write('        GL_CHECK("%s", ' % (specs))
        printFormals(stream, function, True, False, False)
        stream.write(');\n')
    else:
        stream.write('        GL_CHECK();\n')

def genHeader(functionList, headerFile):
    with open(headerFile, 'w') as header:
        # Begin the header
        header.write(textwrap.dedent("""\
        // file automatically generated by %s
        #ifndef R_COMMON_HDR
        #define R_COMMON_HDR
        #include <SDL2/SDL_opengl.h>
        #include <stdint.h>

        #ifdef DEBUG_GL
        #   define GL_INFO const char *file, size_t line
        #   define GL_INFOP , GL_INFO
        #else
        #   define GL_INFO
        #   define GL_INFOP
        #endif

        """) % (__file__))
        # Begin the namespace
        header.write('namespace gl {\n')
        # Generate the function prototypes
        header.write('    void init(void);\n')
        header.write('    bool has(const char *ext);\n');
        for function in functionList:
            header.write('    ')
            printPrototype(header, function, True, True)
        # End the namespace
        header.write('}\n\n')
        # Generate the wrapper macros
        header.write('#if defined(DEBUG_GL) && !defined(R_COMMON_NO_DEFINES)\n')
        largest = max(len(x.name) for x in functionList)
        for f in functionList:
            header.write('    #define %s(...) %s(%s __FILE__, __LINE__)\n'
                % (f.name,
                   f.name.rjust(largest),
                   '__VA_ARGS__,' if len(f.formals) else '/* no arg */')
            )
        # Finish the wrapper macros
        header.write('#endif\n')
        # Finish the header
        header.write('#endif\n')

def genSource(functionList, sourceFile):
    with open(sourceFile, 'w') as source:
        # Begin the source
        source.write(textwrap.dedent("""\
        // File automatically generated by %s
        #include <SDL2/SDL.h> // SDL_GL_GetProcAddress
        #include <stdarg.h>
        #define R_COMMON_NO_DEFINES
        #include "r_common.h"
        #include "util.h"

        #ifndef APIENTRY
        #   define APIENTRY
        #endif
        #ifndef APIENTRYP
        #   define APIENTRYP APIENTRY *
        #endif

        #ifdef DEBUG_GL
        #   define GL_CHECK_0(...)     debugCheck("", __func__, file, line)
        #   define GL_CHECK_1(SP, ...) debugCheck(SP, __func__, file, line)
        #else
        #   define GL_CHECK_0(...)
        #   define GL_CHECK_1(...)
        #endif

        #define PP_PHELP(X, Y) \\
           X ## Y
        #define PP_PASTE(X, Y) \\
           PP_PHELP(X, Y)
        #define PP_SKIP(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, N, ...) \\
           N
        #define PP_SCAN(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, ...) \\
           PP_SKIP(, __VA_ARGS__, _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12)
        #define PP_COUNT(...) \\
           PP_SCAN(12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, __VA_ARGS__)
        #define GL_CHECK_ARG(...) \\
           PP_SCAN(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, __VA_ARGS__)
        #define GL_CHECK(...) \\
           PP_PASTE(GL_CHECK_, GL_CHECK_ARG(__VA_ARGS__))(__VA_ARGS__)

        """) % (__file__))
        # Generate the PFNGL typedefs
        for f in functionList:
            source.write('typedef %s (APIENTRYP PFNGL%sPROC)'
                % (f.type, f.name.upper()))
            printFormals(source, f, False, True)
            source.write(';\n')
        source.write('\n')
        largest = max(len(x.name) for x in functionList)
        for f in functionList:
            fill = largest - len(f.name)
            source.write('static PFNGL%sPROC %s gl%s_ %s= nullptr;\n'
                % (f.name.upper(),
                   ''.rjust(fill),
                   f.name,
                   ''.rjust(fill))
            )
        # Begin DEBUG section
        source.write(textwrap.dedent("""\

        #ifdef DEBUG_GL
        template <char C, typename T>
        u::string stringize(T, char base='?');

        """))
        # Generate all the stringize functions
        for t in types[1:]:
            source.write('template<>\n')
            source.write('u::string stringize<\'%s\', %s>(%s value, char) {\n'
                % (t['spec'], t['name'], t['name']))
            source.write('    return u::format("%s=%s", value);\n'
                % (t['name'], t['format']))
            source.write('}\n')
        # Generate the beginning of the pointer stringize specialization
        source.write(textwrap.dedent("""\
        template <>
        u::string stringize<'*', void *>(void *value, char base) {
            switch (base) {
                case '?': return "unknown";
        """))
        # Generate all cases
        for t in types:
            source.write('        case \'%s\': return u::format("%s*=%%p", value);\n'
                % (t['spec'], t['name']))
        # Finish the pointer stringize specialization and emit the
        # debugErrorString function and beginning of debugCheck function
        source.write(textwrap.dedent("""\
            }

            return u::format("GLchar*=\\"%s\\"", (const char *)value);
        }

        static const char *debugErrorString(GLenum error) {
            switch (error) {
                case GL_INVALID_ENUM:
                    return "GL_INVALID_ENUM";
                case GL_INVALID_VALUE:
                    return "GL_INVALID_VALUE";
                case GL_INVALID_FRAMEBUFFER_OPERATION:
                    return "GL_INVALID_FRAMEBUFFER_OPERATION";
            }
            return "unknown";
        }

        static void debugCheck(const char *spec, const char *function, const char *file, size_t line, ...) {
            GLenum error = glGetError_();
            if (error == GL_NO_ERROR)
                return;

            va_list va;
            va_start(va, line);
            u::string contents;

            for (const char *s = spec; *s; s++) {
                switch (*s) {
        """))
        # Generate all the switches on the spec string
        for t in types[1:]:
            source.write('            case \'%s\':\n' % (t['spec']))
            source.write('                contents += stringize<\'%s\'>((%s)va_arg(va, %s));\n'
                % (t['spec'], t['name'], t['promote']))
            source.write('                break;\n')
        # Emit the end of debugCheck function
        source.write(textwrap.dedent("""\
                    case '*':
                        contents += stringize<'*'>(va_arg(va, void *), s[1]);
                        s++; // skip basetype spec
                        break;
                }
                if (s[1])
                    contents += ", ";
            }
            va_end(va);
            fprintf(stderr, "error %s(%s) (%s:%zu) %s\\n", function, contents.c_str(),
                file, line, debugErrorString(error));
        }
        """))
        # End debug section
        source.write('#endif\n')
        # Begin the namespace
        source.write('\nnamespace gl {\n')
        source.write('    static u::set<u::string> extensions;\n\n');
        # Generate the init function
        source.write('    void init(void) {\n')
        for f in functionList:
            fill = largest - len(f.name)
            source.write('        gl%s_%s = (PFNGL%sPROC)SDL_GL_GetProcAddress("gl%s");\n'
                % (f.name, ''.rjust(fill), f.name.upper(), f.name))
        source.write('\n        GLint count = 0;\n');
        source.write('        glGetIntegerv_(GL_NUM_EXTENSIONS, &count);\n');
        source.write('        for (GLint i = 0; i < count; i++)\n');
        source.write('            extensions.emplace((const char *)glGetStringi_(GL_EXTENSIONS, i));\n');
        source.write('    }\n\n');
        source.write('    bool has(const char *ext) {\n');
        source.write('         return extensions.find(ext) != extensions.end();\n');
        source.write('    }\n');
        # Generate the GL function wrappers
        for f in functionList:
            source.write('\n    %s %s' % (f.type, f.name))
            printFormals(source, f, True, True, True, True)
            source.write(' {\n        ')
            if f.type != 'void':
                # Local backup result and then return it
                source.write('%s result = gl%s_' % (f.type, f.name))
                printFormals(source, f, True, False)
                source.write(';\n')
                printCheck(source, f)
                source.write('        return result;\n    }\n')
            else:
                # Just call
                source.write('gl%s_' % (f.name))
                printFormals(source, f, True, False)
                source.write(';\n')
                printCheck(source, f)
                source.write('    }\n')
        # End the namespace
        source.write('}\n')

def usage(app):
    sys.stdout.write(textwrap.dedent("""\
    usage: %s -g <list> -o <output>
        -g      - OpenGL list
        -o      - output (suffixes with .h and .cpp)
        -h      - help
    """) % (app))
    sys.exit(1)

def main(argv):
    try:
        opts, args = getopt.getopt(argv[1:], "g:o:h:")
    except getopt.GetoptError:
        usage(argv[-1])
        sys.exit()

    protos = ''
    output = ''
    for opt, arg in opts:
        if opt == '-h':
            usage(argv[0])
        elif opt == '-g':
            protos = arg
        elif opt == '-o':
            output = arg
        else:
            usage(argv[0])

    if not protos or not output:
        usage(argv[0])

    functionList = readFunctions(protos)

    genHeader(functionList, '%s.h' % (output))
    genSource(functionList, '%s.cpp' % (output))

if __name__ == "__main__":
    main(sys.argv)