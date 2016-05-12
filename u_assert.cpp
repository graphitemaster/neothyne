#include "engine.h"

namespace u {

[[noreturn]] void assert(const char *expression,
                         const char *file,
                         const char *func,
                         size_t line)
{
    neoFatal("Assertion failed: %s (%s: %s: %d)", expression, file, func, line);
}

}
