#ifndef U_SHA512_HDR
#define U_SHA512_HDR
#include "u_string.h"

namespace u {

struct sha512 {
    sha512();
    sha512(const unsigned char *buf, size_t length);

    void init();
    void process(const unsigned char *src, size_t length);
    void done();

    u::string hex();

private:
    void compress(const unsigned char *buf);

    static const uint64_t K[80];

    static void inline store64(uint64_t x, unsigned char *y) {
        for (int i = 0; i != 8; ++i)
            y[i] = (x >> ((7-i) * 8)) & 255;
    }

    static inline uint64_t load64(const unsigned char *y) {
        uint64_t res = 0;
        for (int i = 0; i != 8; ++i)
            res |= uint64_t(y[i]) << ((7-i) * 8);
        return res;
    }

    static inline uint64_t ch(uint64_t x, uint64_t y, uint64_t z) {
        return z ^ (x & (y ^ z));
    }

    static inline uint64_t maj(uint64_t x, uint64_t y, uint64_t z) {
        return ((x | y) & z) | (x & y);
    }

    static inline uint64_t rot(uint64_t x, uint64_t n) {
        return (x >> (n & 63)) | (x << (64 - (n & 63)));
    }

    static inline uint64_t sh(uint64_t x, uint64_t n) {
        return x >> n;
    }

    static inline uint64_t sigma0(uint64_t x) {
        return rot(x, 28) ^ rot(x, 34) ^ rot(x, 39);
    }

    static inline uint64_t sigma1(uint64_t x) {
        return rot(x, 14) ^ rot(x, 18) ^ rot(x, 41);
    }

    static inline uint64_t gamma0(uint64_t x) {
        return rot(x, 1) ^ rot(x, 8) ^ sh(x, 7);
    }

    static inline uint64_t gamma1(uint64_t x) {
        return rot(x, 19) ^ rot(x, 61) ^ sh(x, 6);
    }

    uint64_t m_length;
    uint64_t m_state[8];
    uint32_t m_currentLength;
    unsigned char m_buffer[128];
    unsigned char m_out[512 / 8];
};

}

#endif
