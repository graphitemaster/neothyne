#include <stdio.h> // vsnprintf
#include <stdarg.h> // va_list, va_copy, va_end

#include "u_memory.h" // unique_ptr

// On implementations where vsnprintf doesn't guarantee a null-terminator will
// be written when the buffer is not large enough (for it), we adjust the
// buffer-size such that we'll have memory to put the null terminator
#if defined(_WIN32) && (!defined(__GNUC__) || __GNUC__ < 4)
static constexpr size_t kSizeCorrection = 1;
#else
static constexpr size_t kSizeCorrection = 0;
#endif
namespace u {
namespace detail {
    int c99vsnprintf(char *str, size_t maxSize, const char *format, va_list ap) {
        va_list cp;
        int ret = -1;
        if (maxSize > 0) {
            va_copy(cp, ap);
            ret = vsnprintf(str, maxSize - kSizeCorrection, format, cp);
            va_end(cp);
            if (ret == int(maxSize - 1))
                ret = -1;
            // MSVCRT does not null-terminate if the result fills the buffer
            str[maxSize - 1] = '\0';
        }
        if (ret != -1)
            return ret;

        // On implementations where vsnprintf returns -1 when the buffer is not
        // large enough, this will iteratively heap allocate memory until the
        // formatting can fit; then return the result of the full format
        // (as per the interface-contract of vsnprintf.)
        if (maxSize < 128)
            maxSize = 128;
        while (ret == -1) {
            maxSize *= 4;
            u::unique_ptr<char[]> data(new char[maxSize]); // Try with a larger buffer
            va_copy(cp, ap);
            ret = vsnprintf(&data[0], maxSize - kSizeCorrection, format, cp);
            va_end(cp);
            if (ret == int(maxSize - 1))
                ret = -1;
        }
        return ret;
    }
}}
