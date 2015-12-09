#include <assert.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif
#if defined(__SSE4_1__)
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
            // this case the base table is set to 0x7C00 (with sign if negative)
            // and the mantissa is zeroed out, which is accomplished by shifting
            // out all mantissa bits.
            baseTable[i|0x000] = 0x7C00;
            baseTable[i|0x100] = 0xFC00;
            shiftTable[i|0x000] = 24;
            shiftTable[i|0x100] = 24;
        } else {
            // Remaining float numbers such as Infs and NaNs should stay Infs and
            // NaNs after conversion. The base table entries are exactly the same
            // as the previous case, except the mantissa-bits are to be preserved
            // as much as possible.
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

float convertToFloat(half in) {
    static constexpr size_t kMagic = 113 << 23;
    static constexpr size_t kShiftedExp = 0x7C00 << 13; // exponent mask after shift

    floatShape out;
    out.asInt = (in & 0x7FFF) << 13; // exponent/mantissa bits
    const size_t exp = kShiftedExp & out.asInt; // exponent
    out.asInt += (127 - 15) << 23; // adjust exponent

    if (exp == kShiftedExp) {
        // extra adjustment of exponent for Inf/Nan?
        out.asInt += (128 - 16) << 23;
    } else if (exp == 0) {
        // extra adjustment of exponent for Zero/Denormal?
        out.asInt += 1 << 23;
        // renormalize
        floatShape magic;
        magic.asInt = kMagic;
        out.asFloat -= magic.asFloat;
    }

    // sign bit
    out.asInt |= (in & 0x8000) << 16;
    return out.asFloat;
}

#if defined(__SSE2__)
union vectorShape {
    __m128i asVector;
    alignas(16) uint32_t asInt[4];
};

template <unsigned int I>
static inline uint32_t extractScalar(__m128i v) {
#if defined(__SSE4_1__)
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

static __m128 convertToFloatSSE2(__m128i h) {
    // ~19 SSE2 ops
    alignas(16) static const uint32_t kMaskNoSign[4] = { 0x7fff, 0x7fff, 0x7fff, 0x7fff };
    alignas(16) static const uint32_t kSmallestNormal[4] = { 0x0400, 0x0400, 0x0400, 0x0400 };
    alignas(16) static const uint32_t kInfinity[4] = { 0x7c00, 0x7c00, 0x7c00, 0x7c00 };
    alignas(16) static const uint32_t kExpAdjustNormal[4] = { (127 - 15) << 23, (127 - 15) << 23, (127 - 15) << 23, (127 - 15) << 23, };
    alignas(16) static const uint32_t kMagicDenormal[4] = { 113 << 23, 113 << 23, 113 << 23, 113 << 23 };

    const __m128i noSign         = *(const __m128i *)&kMaskNoSign;
    const __m128i exponentAdjust = *(const __m128i *)&kExpAdjustNormal;
    const __m128i smallest       = *(const __m128i *)&kSmallestNormal;
    const __m128i infinity       = *(const __m128i *)&kInfinity;
    const __m128i expAnd         = _mm_and_si128(noSign, h);
    const __m128i justSign       = _mm_xor_si128(h, expAnd);
    const __m128i notInfNan      = _mm_cmpgt_epi32(infinity, expAnd);
    const __m128i isDenormal     = _mm_cmpgt_epi32(smallest, expAnd);
    const __m128i shifted        = _mm_slli_epi32(expAnd, 13);
    const __m128i adjustInfNan   = _mm_andnot_si128(notInfNan, exponentAdjust);
    const __m128i adjusted       = _mm_add_epi32(exponentAdjust, shifted);
    const __m128i denormal1      = _mm_add_epi32(shifted, *(const __m128i *)&kMagicDenormal);
    const __m128i adjusted2      = _mm_add_epi32(adjusted, adjustInfNan);
    const __m128  denormal2      = _mm_sub_ps(_mm_castsi128_ps(denormal1), *(const __m128 *)&kMagicDenormal);
    const __m128  adjusted3      = _mm_and_ps(denormal2, _mm_castsi128_ps(isDenormal));
    const __m128  adjusted4      = _mm_andnot_ps(_mm_castsi128_ps(isDenormal), _mm_castsi128_ps(adjusted2));
    const __m128  adjusted5      = _mm_or_ps(adjusted3, adjusted4);
    const __m128i sign           = _mm_slli_epi32(justSign, 16);
    const __m128  value          = _mm_or_ps(adjusted5, _mm_castsi128_ps(sign));

    return value;
}
#endif

u::vector<half> convertToHalf(const float *const in, size_t length) {
    u::vector<half> result(length);

    assert(((uintptr_t)(const void *)in) % 16 == 0);

#if defined(__SSE2__)
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
    for (int i = 0; i < remainder; i++)
        result[where+i] = convertToHalf(in[where+i]);
#else
    for (size_t i = 0; i < length; i++)
        result[i] = convertToHalf(in[i]);
#endif
    return result;
}

u::vector<float> convertToFloat(const half *const in, size_t length) {
    u::vector<float> result(length);

#if defined(__SSE2__)
    const int blocks = int(length) / 4;
    const int remainder = int(length) % 4;
    int where = 0;
    for (int i = 0; i < blocks; i++, where += 4) {
        alignas(16) const __m128i value = _mm_set_epi32(in[where+0], in[where+1], in[where+2], in[where+3]);
        const __m128 convert = convertToFloatSSE2(value);
        memcpy(&result[where], &convert, sizeof convert);
    }
    for (int i = 0; i < remainder; i++)
        result[where+i] = convertToFloat(in[where+i]);
#else
    for (size_t i = 0; i < length; i++)
        result[i] = convertToFloat(in[i]);
#endif
    return result;
}

}
