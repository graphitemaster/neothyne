#include <time.h> // time_t, time

#include "u_misc.h"
#include "u_memory.h" // unique_ptr

namespace u {
namespace detail {
    // On implementations where vsnprintf doesn't guarantee a null-terminator will
    // be written when the buffer is not large enough (for it), we adjust the
    // buffer-size such that we'll have memory to put the null terminator
#if defined(_WIN32) && (!defined(__GNUC__) || __GNUC__ < 4)
    static constexpr size_t kSizeCorrection = 1;
#else
    static constexpr size_t kSizeCorrection = 0;
#endif
    int c99vsnprintf(char *str, size_t maxSize, const char *format, va_list ap) {
#ifdef _WIN32
        // MSVCRT does not support %zu, so we convert it to %Iu
        u::string fmt = format;
        fmt.replace_all("%zu", "%Iu");
#else
        const char *fmt = format;
#endif

        va_list cp;
        int ret = -1;
        if (maxSize > 0) {
            va_copy(cp, ap);
            ret = vsnprintf(str, maxSize - kSizeCorrection, &fmt[0], cp);
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
            ret = vsnprintf(&data[0], maxSize - kSizeCorrection, &fmt[0], cp);
            va_end(cp);
            if (ret == int(maxSize - 1))
                ret = -1;
        }
        return ret;
    }

    int c99vsscanf(const char *s, const char *format, va_list ap) {
#ifdef _WIN32
        u::string fmt = format;
        fmt.replace_all("%zu", "%Iu");
#else
        const char *fmt = format;
#endif
        return vsscanf(s, &fmt[0], ap);
    }
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

}
