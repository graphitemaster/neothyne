#include <stdint.h>

#include "u_hash.h"
#include "u_traits.h"

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSSE3__)
#include <tmmintrin.h>
#endif

namespace u {

#if defined(__SSE2__)
static void hash128(const unsigned char *buffer, size_t bufferSize, unsigned char *out)
{
    const unsigned char *const end = (const unsigned char *const )buffer + bufferSize;
    const unsigned char *p = buffer;
    const size_t toAligned = ((size_t(p) % 16) == 0) ? 0 : 16 - (size_t(p) % 16);
    alignas(16) uint32_t state[] = { 5381, 5381, 5381, 5381 };

    // hash input until pointer reaches alignment
    size_t s = 0;
    for (size_t i = 0; p < end && i < toAligned; i++, p++) {
        state[s] = (state[s] << 5) + state[s] + *p;
        s = (s+1) & 0x03;
    }

    // rotate state vector for SIMD processing
    U_ASSUME_ALIGNED(p, 16);
    for (size_t i = 0; i < s; i++) {
        const size_t rotate = state[0];
        state[0] = state[1];
        state[1] = state[2];
        state[2] = state[3];
        state[3] = rotate;
    }

    __m128i xstate = _mm_load_si128((__m128i *)state);
#if defined(__SSSE3__)
    const __m128i xbmask = _mm_set1_epi32(0x000000FF);
    const __m128i xshuffle = _mm_set_epi8(15, 11, 7, 3,
                                          14, 10, 6, 2,
                                          13, 9,  5, 1,
                                          12, 8,  4, 0);
#else
    __m128i xqword;
#endif
    __m128i xpin;
    __m128i xp;

    // hash input
    for (; p+15 < end; p += 16) {
        U_ASSUME_ALIGNED(p, 16);
        xpin = _mm_load_si128((__m128i *)p);

#if defined(__SSSE3__)
#   define hashRound(ROUND) \
        do { \
            xp = _mm_srli_epi32(xpin, 8*(ROUND)); \
            xp = _mm_and_si128(xp, xbmask); \
            xp = _mm_add_epi32(xp, xstate); \
            xstate = _mm_slli_epi32(xstate, 5); \
            xstate = _mm_add_epi32(xstate, xp); \
        } while (0)

        xpin = _mm_shuffle_epi8(xpin, xshuffle);

        hashRound(0);
        hashRound(1);
        hashRound(2);
        hashRound(3);
#else
#   define hashRound() \
        do { \
            xp = _mm_add_epi32(xp, xstate); \
            xstate = _mm_slli_epi32(xstate, 5); \
            xstate = _mm_add_epi32(xstate, xp); \
        } while (0)

        // qword0: low
        xqword = _mm_setzero_si128();
        xqword = _mm_unpacklo_epi8(xpin, xqword);
        // dword0
        xp = _mm_setzero_si128();
        xp = _mm_unpacklo_epi8(xqword, xp);
        hashRound();
        // dword1
        xp = _mm_setzero_si128();
        xp = _mm_unpackhi_epi8(xqword, xp);
        hashRound();

        // qword1: high
        xqword = _mm_setzero_si128();
        xqword = _mm_unpackhi_epi8(xpin, xqword);
        // dword2
        xp = _mm_setzero_si128();
        xp = _mm_unpacklo_epi8(xqword, xp);
        hashRound();
        // dword 3
        xp = _mm_setzero_si128();
        xp = _mm_unpackhi_epi8(xqword, xp);
        hashRound();
#endif
    }

    _mm_store_si128((__m128i *)state, xstate);

    // rotate back
    for (size_t i = 0; i < s; i++) {
        const size_t rotate = state[3];
        state[3] = state[2];
        state[2] = state[1];
        state[1] = state[0];
        state[0] = rotate;
    }

    // process the tail
    for (; p < end; p++) {
        state[s] = (state[s] << 5) + state[s] + *p;
        s = (s+1) & 0x03;
    }

    memcpy(out, state, sizeof state);
}
#else
static void hash128(const unsigned char *buffer, size_t bufferSize, unsigned char *out)
{
    const unsigned char *const end = (const unsigned char *const )buffer + bufferSize;
    uint32_t state[] = { 5381, 5381, 5381, 5381 };
    size_t s = 0;
    for (const unsigned char *p = buffer; p < end; p++) {
        state[s] = state[s] * 33  + *p;
        s = (s+1) & 0x03;
    }
    memcpy(out, state, sizeof state);
}
#endif

djbx33a::djbx33a(const unsigned char *data, size_t size) {
    unsigned char output[16];
    hash128(data, size, output);
    char *hexString = m_result;
    static const char *kHex = "0123456789ABCDEF";
    for (size_t i = 0; i < sizeof output; ++i) {
        *hexString++ = kHex[(output[i] >> 4) & 0x0F];
        *hexString++ = kHex[output[i] & 0x0F];
    }
    m_result[16] = '\0';
}

}
