# Represents a function
class function:
    def __init__(self):
        self.formals = []
    type = ''       # Type of the function
    name = ''       # Name of the function
    formals = None  # List of tuples containing (type, name)

# Parses a file containing prototypes using the following grammar
#   <type> ":" <ident> "(" <formals> ")" <terminator>
# Where
#   <terminator>    ::= <EOL> | ";" <EOL>
#   <formals>       ::= <empty> | <formal> | <formal> "," <formal>
#   <formal>        ::= <type> ":" <ident>
#   <EOL>           ::= "\r\n" | "\n"
# Example
#   void: foo(int: x, float: y);
def readPrototypes(functionFile):
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

# Prints the type and name of a function
def printTypeName(stream, function, name=True):
    if name:
        stream.write('%s %s' % (function.type, function.name))
    else:
        stream.write('%s' % (function.type))

# Prints the formals of a function
def printFormals(stream, function, names=True, types=True, parens=True, info=''):
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
        stream.write(info)
    if parens:
        stream.write(')')

# Prints the full prototype of a function
def printPrototype(stream, function, info='', semicolon=False):
    printTypeName(stream, function, True)
    printFormals(stream, function, True, True, True, info)
    stream.write(';\n' if semicolon else '\n')
