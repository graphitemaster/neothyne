#include "engine.h"
#include "u_assert.h"

namespace u {
namespace detail {
void assertFail(const char *fmt, va_list va) {
    neoFatal(u::format(fmt, va).c_str());
}
}
}
