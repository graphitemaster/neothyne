#include <SDL_cpuinfo.h>
#include <SDL_endian.h>

#include <time.h> // time_t, time
#include <float.h>
#include <limits.h>

#include "u_misc.h"
#include "u_memory.h" // unique_ptr

namespace u {

union CheckEndian {
    constexpr CheckEndian(uint32_t value)
        : asUint32(value)
    {
    }
    uint32_t asUint32;
    uint8_t asBytes[sizeof asUint32];
};

static constexpr CheckEndian kCheckEndian(0x01020304);

bool isLilEndian() {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
    return true;
#endif
    return kCheckEndian.asBytes[0] == 0x04;
}

bool isBigEndian() {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    return true;
#endif
    return kCheckEndian.asBytes[0] == 0x01;
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

#if defined(__GNUC__)
#   include <cpuid.h>
#   define CPUID(FUNC, A, B, C, D) \
        __get_cpuid((FUNC), &A, &B, &C, &D)
#elif defined(_MSC_VER)
#   define CPUID(FUNC, A, B, C, D) \
        do { \
            int info[4]; \
            __cpuid(info, (FUNC)); \
            A = info[0]; \
            B = info[1]; \
            C = info[2]; \
            D = info[3]; \
        } while (0)
#else
#   define CPUID(FUNC, A, B, C, D) \
        ([]() { static_assert(0, "no suitable implementation of CPUID found"); })()
#endif

const char *CPUDesc() {
    static char desc[1024];
    if (desc[0])
        return desc;
    int i = 0;
    unsigned int a = 0, b = 0, c = 0, d = 0;
    CPUID(0x80000000, a, b, c, d);
    if (a >= 0x80000004) {
        for (int k = 0; k < 3; k++) {
            CPUID(0x80000002u + k, a, b, c, d);
            for (int j = 0; j < 4; j++) { desc[i++] = (char)(a & 0xFF); a >>= 8; }
            for (int j = 0; j < 4; j++) { desc[i++] = (char)(b & 0xFF); b >>= 8; }
            for (int j = 0; j < 4; j++) { desc[i++] = (char)(c & 0xFF); c >>= 8; }
            for (int j = 0; j < 4; j++) { desc[i++] = (char)(d & 0xFF); d >>= 8; }
        }
    }
    if (desc[0]) {
        // trim leading whitespace
        char *trim = desc;
        while (u::isspace(*trim)) trim++;
        if (trim != desc && *trim)
            u::moveMemory(desc, trim, strlen(trim) + 1);
        // trim trailing whitespace
        const size_t size = strlen(desc);
        char *end = desc + size - 1;
        while (end >= desc && u::isspace(*end))
            end--;
        *(end + 1) = '\0';
        // replace instances of two or more spaces with one
        char *dest = desc;
        char *chop = desc;
        while (*chop) {
            while (*chop == ' ' && *(chop + 1) == ' ')
                chop++;
            *dest++ = *chop++;
        }
        *dest ='\0';

        char format[1024];
        int count = SDL_GetCPUCount();
        int wrote = snprintf(format, sizeof format, "%s (%d %s)",
            desc, count, count > 1 ? "cores" : "core");
        if (wrote == -1)
            return desc;
        memcpy(desc, format, wrote + 1);
        return desc;
    }
    snprintf(desc, sizeof desc, "Unknown");
    return desc;
}

const char *RAMDesc() {
    static char desc[128];
    if (desc[0])
        return desc;
    // SDL reports MB, sizeMetric expects bytes
    snprintf(desc, sizeof desc, "%s", u::sizeMetric(size_t(SDL_GetSystemRAM()) * (1u << 20)).c_str());
    return desc;
}

static struct crc32Table {
    crc32Table() {
        // Build the initial CRC32 table
        for (size_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (size_t j = 0; j < 8; j++)
                c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
            m_table[0][i] = c;
        }
        // Build the others for slicing-by-8
        for (size_t i = 0; i < 256; i++) {
            uint32_t c = m_table[0][i];
            for (size_t j = 1; j < 8; j++) {
                c = m_table[0][c & 0xFF] ^ (c >> 8);
                m_table[j][i] = c;
            }
        }
    }
    uint32_t operator()(size_t i, size_t j) const {
        return m_table[i][j];
    }
private:
    uint32_t m_table[8][256];
} gCRC32;

// based on the slicing-by-8 algorithm as described in
// "High Octane CRC Generation with the Intel Slicing-by-8 Algorithm"
uint32_t crc32(const unsigned char *buffer, size_t length) {
    // Align to 8 for better performance
    uint32_t crc = ~0u;
    for (; length && ((uintptr_t)buffer & 7); length--, buffer++)
        crc = gCRC32(0, (crc ^ *buffer) & 0xFF) ^ (crc >> 8);
    for (; length >= 8; length -= 8, buffer += 8) {
        uint32_t word;
        memcpy(&word, buffer, sizeof word);
        u::endianSwap(&word, 1);
        crc ^= word;
        crc = gCRC32(7, crc & 0xFF) ^
              gCRC32(6, (crc >> 8) & 0xFF) ^
              gCRC32(5, (crc >> 16) & 0xFF) ^
              gCRC32(4, (crc >> 24) & 0xFF) ^
              gCRC32(3, buffer[4]) ^
              gCRC32(2, buffer[5]) ^
              gCRC32(1, buffer[6]) ^
              gCRC32(0, buffer[7]);
    }
    for (; length; length--, buffer++)
        crc = gCRC32(0, (crc ^ *buffer) & 0xFF) ^ (crc >> 8);
    return ~crc;
}

}

#if defined(_MSC_VER)
// Visual Studio 2015+ adds telemetry calls into the resulting binary which
// can only be disabled by linking an object or adding some empty stubs to
// the code.
//
// The optimizer should eliminate these calls.
//
// https://www.reddit.com/r/cpp/comments/4ibauu/visual_studio_adding_telemetry_function_calls_to
extern "C" {
    void _cdecl __vcrt_initialize_telemetry_provider() { }
    void _cdecl __telemetry_main_invoke_trigger() { }
    void _cdecl __telemetry_main_return_trigger() { }
    void _cdecl __vcrt_uninitialize_telemetry_provider() { }
};
#endif
