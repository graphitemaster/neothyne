#include <limits.h> // UINT32_MAX
#include <stddef.h> // size_t
#include <time.h>
#include <stdio.h>

namespace u {

static constexpr size_t kSize = 624;
static constexpr size_t kPeriod = 397;
static constexpr size_t kDiff = kSize - kPeriod;
static constexpr uint32_t kMatrix[2] = { 0, 0x9908B0DF };

// State for Mersenne Twister
static struct state {
    uint32_t mt[kSize];
    size_t index;
    state() {
        union {
            time_t t;
            uint32_t u;
        } seed = { time(nullptr) };
        mt[0] = seed.u;
        index = 0;
        for (size_t i = 1; i < kSize; ++i)
            mt[i] = 0x6C078965 * (mt[i-1] ^ mt[i-1] >> 30) + i;
    }
} gState;

#define gMT (gState.mt)
#define gIndex (gState.index)

static inline uint32_t m32(uint32_t x) {
    return 0x80000000 & x;
}

static inline uint32_t l31(uint32_t x) {
    return 0x7FFFFFFF & x;
}

static inline bool odd(uint32_t x) {
    return x & 1; // Check if number is odd
}

static inline void generateNumbers() {
    size_t y;
    size_t i;

    // i = [0 ... 226]
    for (i = 0; i < kDiff; ++i) {
        y = m32(gMT[i]) | l31(gMT[i+1]);
        gMT[i] = gMT[i+kPeriod] ^ (y>>1) ^ kMatrix[odd(y)];
    }

    auto unroll = [&y, &i]() {
        y = m32(gMT[i]) | l31(gMT[i+1]);
        gMT[i] = gMT[i-kDiff] ^ (y>>1) ^ kMatrix[odd(y)];
        ++i;
    };

    // i = [227 ... 622]
    for (i = kDiff; i < kSize-1; ) {
        for (size_t j = 0; j < 11; j++)
            unroll();
    }

    // i = [623]
    y = m32(gMT[kSize-1]) | l31(gMT[kSize-1]);
    gMT[kSize-1] = gMT[kPeriod-1] ^ (y>>1) ^ kMatrix[odd(y)];
}


uint32_t randu() {
    if (gIndex == 0)
        generateNumbers();
    uint32_t y = gMT[gIndex];

    // Tempering
    y ^= y >> 11;
    y ^= y << 7 & 0x9D2C5680;
    y ^= y << 15 & 0xEFC60000;
    y ^= y >> 18;

    if (++gIndex == kSize)
        gIndex = 0;
    return y;
}

float randf() {
    return float(randu()) / UINT32_MAX;
}

}
