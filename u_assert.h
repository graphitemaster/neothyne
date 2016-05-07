#ifndef U_ASSERT_HDR
#define U_ASSERT_HDR
#include <stdarg.h>

#if defined(assert)
#   undef assert
#endif

#if defined(__GNUC__)
#   define U_ASSERT_FUNCTION __PRETTY_FUNCTION__
#else
#   define U_ASSERT_FUNCTION __func__
#endif

namespace u {

namespace detail {
#if defined(NDEBUG)
    template <typename... V>
    static inline void assertCheck(bool, const V &...) {
        // Do nothing
    }
#else
    void assertFail(const char *fmt, va_list va);

    template <typename... V>
    static inline void assertCheck(bool expr, ...) {
        if (!expr) {
            va_list va;
            va_start(va, expr);
            assertFail("Assertion failure: %s (%s: %s %d)", va);
            va_end(va);
        }
    }
#endif
}

}

#define assert(EXPRESSION) \
    detail::assertCheck(!!(EXPRESSION), #EXPRESSION, __FILE__, U_ASSERT_FUNCTION, __LINE__)

#endif
