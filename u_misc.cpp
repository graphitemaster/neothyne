#include <time.h> // time_t, time
#include <float.h>
#include <limits.h>

#include "u_misc.h"
#include "u_memory.h" // unique_ptr

namespace u {
namespace detail {
#if defined(_WIN32)
    // Performs replacements on the format string into ones understood
    // by MSVCRT.
    u::string fixFormatString(const char *format) {
        u::string replace(format);
        struct { const char *find, *replace; } replacements[] = {
            { "%zd", "%Id" },
            { "%zi", "%Ii" },
            { "%zo", "%Io" },
            { "%zu", "%Iu" },
            { "%zx", "%Ix" }
        };
        for (const auto &it : replacements)
            replace.replace_all(it.find, it.replace);
        return replace;
    }
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900
    int c99vsnprintf(char *out, size_t size, const char *string, va_list ap) {
        u::string format = fixFormatString(string);
        int count = -1;
        if (size != 0)
            count = _vsnprintf_s(out, size, _TRUNCATE, &format[0], ap);
        if (count == -1)
            count = _vscprintf(&format[0], ap);
        return count;
    }
#elif defined(_MSC_VER)
    int c99vsnprintf(char *out, size_t size, const char *string, va_list ap) {
        // Visual Studio 2015 fixes _vsnprintf_s
        u::string format = fixFormatString(string);
        return _vsnprintf_s(out, size, &format[0], ap);
    }
#else
    int c99vsnprintf(char *out, size_t size, const char *string, va_list ap) {
    // Older MSVCRTs get the truncation wrong. The standard requires that
    // a null terminator be written even if the buffer size cannot accommodate
    // the null terminator. This size correction deals with giving us an
    // additional character of space to do the null termination.
#if defined (_WIN32) || (!defined(__GNUC__) || __GNUC__ < 4)
        static constexpr size_t kSizeCorrection = 1;
        u::string format = fixFormatString(string);
#else
        static constexpr size_t kSizeCorrection = 0;
        const char *format = string;
#endif
        va_list copy;
        int ret = -1;
        if (size > 0) {
            va_copy(copy, ap);
            ret = vsnprintf(out, size - kSizeCorrection, &format[0], copy);
            va_end(copy);
            if (ret == int(size-1))
                ret = -1;
            // Write the null terminator since vsnprintf may not
            out[size-1] = '\0';
        }
        if (ret != -1)
            return ret;

        // Keep resizing until the format succeeds
        char *s = nullptr;
        if (size < 128)
            size = 128;
        while (ret == -1) {
            size *= 4;
            s = neoRealloc(s, size);
            va_copy(copy, ap);
            ret = vsnprintf(s, size - kSizeCorrection, &format[0], copy);
            va_end(copy);
            if (ret == int(size-1))
                ret = -1;
        }
        neoFree(s);
        return ret;
    }
#endif

#if defined(_WIN32)
    int c99vsscanf(const char *out, const char *string, va_list ap) {
        u::string format = fixFormatString(format);
        return vsscanf(out, &format[0], ap);
    }
#else
    int c99vsscanf(const char *out, const char *format, va_list ap) {
        // Sane platforms
        return vsscanf(out, format, ap);
    }
#endif
}

void *moveMemory(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s)
        return d;
    if (s+n <= d || d+n <= s)
        return memcpy(d, s, n);
    if (d < s) {
        if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
            while ((uintptr_t)d % sizeof(size_t)) {
                if (!n--)
                    return dest;
                *d++ = *s++;
            }
            for (; n >= sizeof(size_t); n -= sizeof(size_t), d += sizeof(size_t), s += sizeof(size_t))
                *(size_t*)d = *(size_t*)s;
        }
        for (; n; n--)
            *d++ = *s++;
    } else {
        if ((uintptr_t)s % sizeof(size_t) == (uintptr_t)d % sizeof(size_t)) {
            while ((uintptr_t)(d+n) % sizeof(size_t)) {
                if (!n--)
                    return dest;
                d[n] = s[n];
            }
            while (n >= sizeof(size_t)) {
                n -= sizeof(size_t);
                *(size_t*)(d+n) = *(size_t*)(s+n);
            }
        }
        while (n) {
            n--;
            d[n] = s[n];
        }
    }
    return dest;
}

static inline uint32_t m32(uint32_t x) {
    return 0x80000000 & x;
}

static inline uint32_t l31(uint32_t x) {
    return 0x7FFFFFFF & x;
}

static inline bool odd(uint32_t x) {
    return x & 1; // Check if number is odd
}

// State for Mersenne Twister
static struct mtState {
    static constexpr size_t kSize = 624;
    static constexpr size_t kPeriod = 397;
    static constexpr size_t kDiff = kSize - kPeriod;
    static constexpr uint32_t kMatrix[2] = { 0, 0x9908B0DF };

    mtState();
    uint32_t randu();
    float randf();

protected:
    void generate();

private:
    uint32_t m_mt[kSize];
    size_t m_index;
} gRandomState;

constexpr uint32_t mtState::kMatrix[2];

inline mtState::mtState() :
    m_index(0)
{
    union {
        time_t t;
        uint32_t u;
    } seed = { time(nullptr) };
    m_mt[0] = seed.u;
    for (size_t i = 1; i < kSize; ++i)
        m_mt[i] = 0x6C078965u * (m_mt[i-1] ^ m_mt[i-1] >> 30) + i;
}

inline void mtState::generate() {
    size_t y;
    size_t i;

    // i = [0 ... 226]
    for (i = 0; i < kDiff; ++i) {
        y = m32(m_mt[i]) | l31(m_mt[i+1]);
        m_mt[i] = m_mt[i+kPeriod] ^ (y>>1) ^ kMatrix[odd(y)];
    }

    // i = [227 ... 622]
    for (i = kDiff; i < kSize-1; ) {
        for (size_t j = 0; j < 11; j++, ++i) {
            y = m32(m_mt[i]) | l31(m_mt[i+1]);
            m_mt[i] = m_mt[i-kDiff] ^ (y>>1) ^ kMatrix[odd(y)];
        }
    }

    // i = [623]
    y = m32(m_mt[kSize-1]) | l31(m_mt[kSize-1]);
    m_mt[kSize-1] = m_mt[kPeriod-1] ^ (y>>1) ^ kMatrix[odd(y)];
}

inline uint32_t mtState::randu() {
    if (m_index == 0)
        generate();

    uint32_t y = m_mt[m_index];

    // Tempering
    y ^= y >> 11;
    y ^= y << 7 & 0x9D2C5680;
    y ^= y << 15 & 0xEFC60000;
    y ^= y >> 18;

    if (++m_index == kSize)
        m_index = 0;
    return y;
}

inline float mtState::randf() {
    return float(randu()) / UINT32_MAX;
}

uint32_t randu() {
    return gRandomState.randu();
}

float randf() {
    return gRandomState.randf();
}

unsigned char log2(uint32_t v) {
    // 32-byte table
    static const unsigned char kMultiplyDeBruijnBitPosition[32] = {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
    return kMultiplyDeBruijnBitPosition[(uint32_t)(v * 0x077CB531U) >> 27];
}

}
