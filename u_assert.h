#ifndef U_ASSERT_HDR
#define U_ASSERT_HDR
#include <stddef.h>

#if defined(assert)
#   undef assert
#endif

#if defined(__GNUC__)
#   define U_ASSERT_FUNCTION __PRETTY_FUNCTION__
#else
#   define U_ASSERT_FUNCTION __func__
#endif

namespace u {

[[noreturn]] void assert(const char *expression, const char *file, const char *func, size_t line);

}

#if defined(NDEBUG)
#   define U_ASSERT(X) (void)0
#else
#   define U_ASSERT(EXPRESSION) \
        ((void)((EXPRESSION) || (u::assert(#EXPRESSION, __FILE__, U_ASSERT_FUNCTION, __LINE__), 0)))
#endif

#endif
