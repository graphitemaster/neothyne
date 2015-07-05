#ifdef __SSE2__
#include <emmintrin.h>
#endif

#ifdef __SSE4_1__
#include <smmintrin.h>
#endif

#include "m_half.h"
#include "m_const.h"

namespace m {

static struct halfData {
    halfData();
    // 1536 bytes
    uint32_t baseTable[512];
    uint8_t shiftTable[512];
} gHalf;

halfData::halfData() {
    for (int i = 0, e = 0; i < 256; ++i) {
        e = i - 127;
        if (e < -24) {
            // When magnitude of the number is really small (2^-24 or smaller),
            // there is no possible half-float representation for the number, so
            // it must be mapped to zero (or negative zero). Setting the shift
            // table entries to 24 will shift all mantissa bits, leaving just zero.
            // Base tables store zero otherwise (0x8000 for negative zero case.)
            baseTable[i|0x000] = 0x0000;
            baseTable[i|0x100] = 0x8000;
            shiftTable[i|0x000] = 24;
            shiftTable[i|0x100] = 24;
        } else if (e < -14) {
            // When the number is small (< 2^-14), the value can only be
            // represented using a subnormal half-float. This is the most
            // complex case: first, the leading 1-bit, implicitly represented
            // in the normalized representation, must be explicitly added, then
            // the resulting mantissa must be shifted rightward, or over a number
            // of bit-positions as determined by the exponent. Here we prefer to
            // shift the original mantissa bits, and add the pre-shifted 1-bit to
            // it.
            //
            // Note: Shifting by -e-14 will never be negative, thus ensuring
            // proper conversion, an alternative method is to shift by 18-e, which
            // depends on implementation-defined behavior of unsigned shift.
            baseTable[i|0x000] = (0x0400 >> (-e-14));
            baseTable[i|0x100] = (0x0400 >> (-e-14)) | 0x8000;
            shiftTable[i|0x000] = -e-1;
            shiftTable[i|0x100] = -e-1;
        } else if (e <= 15) {
            // Normal numbers (smaller than 2^15), can be represented using half
            // floats, albeit with slightly less precision. The entries in the
            // base table are simply set to the bias-adjust exponent value, shifted
            // into the right position. A sign bit is added for the negative case.
            baseTable[i|0x000] = ((e+15) << 10);
            baseTable[i|0x100] = ((e+15) << 10) | 0x8000;
            shiftTable[i|0x000] = 13;
            shiftTable[i|0x100] = 13;
        } else if (e < 128) {
            // Large values (numbers less than 2^128) must be mapped to half-float
            // Infinity, They are too large to be represented as half-floats. In
            // this case the base table is set to 0x&c00 (with sign if negative)
            // and the mantissa is zeroed out, which is accomplished by shifting
            // out all mantissa bits.
            baseTable[i|0x000] = 0x7C00;
            baseTable[i|0x100] = 0xFC00;
            shiftTable[i|0x000] = 24;
            shiftTable[i|0x100] = 24;
        } else {
            // Remaining float numbers such as Infs and NaNs should stay Infs and
            // NaNs after conversion. The base table entries is exactly the same
            // as the previous case, except the mantissa-bits are to be preserved
            // as much as possible, thus ensuring Infs and NaNs preserve as well.
            baseTable[i|0x000] = 0x7C00;
            baseTable[i|0x100] = 0xFC00;
            shiftTable[i|0x000] = 13;
            shiftTable[i|0x100] = 13;
        }
    }
}

half convertToHalf(float in) {
    floatShape shape;
    shape.asFloat = in;
    return gHalf.baseTable[(shape.asInt >> 23) & 0x1FF] +
        ((shape.asInt & 0x007FFFFF) >> gHalf.shiftTable[(shape.asInt >> 23) & 0x1FF]);
}

#ifdef __SSE2__
union vectorShape {
    __m128i asVector;
    alignas(16) uint32_t asInt[4];
};

template <unsigned int I>
static inline int extractScalar(__m128i v) {
#ifdef __SSE4_1__
    return _mm_extract_epi32(v, I);
#else
    // Not pretty
    vectorShape shape;
    shape.asVector = v;
    return shape.asInt[I];
#endif
}

static __m128i convertToHalfSSE2(__m128 f) {
    // ~15 SSE2 ops
    alignas(16) static const uint32_t kMaskAbsolute[4] = { 0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu };
    alignas(16) static const uint32_t kInf32[4] = { 255 << 23, 255 << 23, 255 << 23, 255 << 23 };
    alignas(16) static const uint32_t kExpInf[4] = { (255 ^ 31) << 23, (255 ^ 31) << 23, (255 ^ 31) << 23, (255 ^ 31) << 23 };
    alignas(16) static const uint32_t kMax[4] = { (127 + 16) << 23, (127 + 16) << 23, (127 + 16) << 23, (127 + 16) << 23 };
    alignas(16) static const uint32_t kMagic[4] = { 15 << 23, 15 << 23, 15 << 23, 15 << 23 };

    const __m128  maskAbsolute = *(const __m128 *)&kMaskAbsolute;
    const __m128  absolute     = _mm_and_ps(maskAbsolute, f);
    const __m128  justSign     = _mm_xor_ps(f, absolute);
    const __m128  max          = *(const __m128 *)&kMax;
    const __m128  expInf       = *(const __m128 *)&kExpInf;
    const __m128  infNanCase   = _mm_xor_ps(expInf, absolute);
    const __m128  clamped      = _mm_min_ps(max, absolute);
    const __m128  notNormal    = _mm_cmpnlt_ps(absolute, *(const __m128 *)&kInf32);
    const __m128  scaled       = _mm_mul_ps(clamped, *(const __m128 *)&kMagic);
    const __m128  merge1       = _mm_and_ps(infNanCase, notNormal);
    const __m128  merge2       = _mm_andnot_ps(notNormal, scaled);
    const __m128  merged       = _mm_or_ps(merge1, merge2);
    const __m128i shifted      = _mm_srli_epi32(_mm_castps_si128(merged), 13);
    const __m128i signShifted  = _mm_srli_epi32(_mm_castps_si128(justSign), 16);
    const __m128i value        = _mm_or_si128(shifted, signShifted);

    return value;
}
#endif

u::vector<half> convertToHalf(const float *in, size_t length) {
    u::vector<half> result(length);
#ifdef __SSE2__
    const int blocks = int(length) / 4;
    const int remainder = int(length) % 4;
    int where = 0;
    for (int i = 0; i < blocks; i++) {
        const __m128 value = _mm_load_ps(&in[where]);
        const __m128i convert = convertToHalfSSE2(value);
        result[where++] = extractScalar<0>(convert);
        result[where++] = extractScalar<1>(convert);
        result[where++] = extractScalar<2>(convert);
        result[where++] = extractScalar<3>(convert);
    }
    for (int i = 0; i < remainder; i++) {
        result[where+i] = convertToHalf(in[where+i]);
        where++;
    }
#else
    for (size_t i = 0; i < length; i++)
        result[i] = convertToHalf(in[i]);
#endif
    return result;
}

}
