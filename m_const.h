#ifndef M_CONST_HDR
#define M_CONST_HDR
#include <stdint.h>
#include <assert.h>
#include <float.h>

namespace m {

const float kPi        = 3.1415926535f;
const float kTau       = kPi * 2.0f;
const float kPiHalf    = kPi * 0.5f;
const float kSqrt2Half = 0.707106781186547f;
const float kSqrt2     = 1.4142135623730950488f;
const float kEpsilon   = 0.00001f;
const float kDegToRad  = kPi / 180.0f;
const float kRadToDeg  = 180.0f / kPi;

inline float toRadian(float x) { return x * kDegToRad; }
inline float toDegree(float x) { return x * kRadToDeg; }

inline float angleMod(float angle) {
    static const float f = 65536.0f / 360.0f;
    static const float i = 360.0f / 65536.0f;
    return i * ((int)(angle * f) & 65535);
}

template <typename T>
inline T clamp(const T& current, const T &min, const T &max) {
    return (current > max) ? max : ((current < min) ? min : current);
}

union floatShape {
    float asFloat;
    uint32_t asInt;
};

inline float abs(float v) {
    floatShape s = { v };
    s.asInt &= 0x7FFFFFFF;
    return s.asFloat;
}

///! Inlinable trigonometry
static const double kC1PIO2 = 1*kPiHalf;
static const double kC2PIO2 = 2*kPiHalf;
static const double kC3PIO2 = 3*kPiHalf;
static const double kC4PIO2 = 4*kPiHalf;

// On i386 the x87 FPU does everything in 80 bits of precision. Using a double
// for the reduced range approximations would cause the compiler to generate
// additional load and stores to reduce the 80bit results to 64bit ones. By using
// the effective type floating-point arithmetic is evaluated on in hardware we
// can eliminate those.
#if FLOAT_EVAL_METHOD == 2
typedef long double f64;
typedef f64 f32;
#else
typedef double f64;
typedef float f32;
#endif

// |cos(x) - c(x)| < 2**-34.1 (~[-5.37e-11, 5.295e-11])
static inline float cosdf(double x) {
    static const double kC0 = -0x1FFFFFFD0C5E81.0p-54; // -0.499999997251031003120
    static const double kC1 = 0x155553E1053A42.0p-57; // 0.0416666233237390631894
    static const double kC2 = -0x16C087E80F1E27.0p-62; // -0.00138867637746099294692
    static const double kC3 = 0x199342E0EE5069.0p-68; // 0.0000243904487962774090654
    const f64 z = x*x;
    const f64 w = z*z;
    const f64 r = kC2+z*kC3;
    return ((1.0+z*kC0) + w*kC1) + (w*z)*r;
}

// |sin(x)/x - s(x)| < 2**-37.5 (~[-4.89e-12, 4.824e-12])
static inline float sindf(double x) {
    static const double kS1 = -0x15555554CBAC77.0p-55; // -0.166666666416265235595
    static const double kS2 = 0x111110896EFBB2.0p-59; // 0.0083333293858894631756
    static const double kS3 = -0x1A00F9E2CAE774.0p-65; // -0.000198393348360966317347
    static const double kS4 = 0x16CD878C3B46A7.0p-71; // 0.0000027183114939898219064
    const f64 z = x*x;
    const f64 w = z*z;
    const f64 r = kS3 + z*kS4;
    const f64 s = z*x;
    return (x + s*(kS1 + z*kS2)) + s*w*r;
}

// |tan(x)/x - t(x)| < 2**-25.5 (~[-2e-08, 2e-08])
static inline float tandf(double x, bool odd) {
    static const double kT0 = 0x15554D3418C99F.0p-54; // 0.333331395030791399758
    static const double kT1 = 0x1112FD38999F72.0p-55; // 0.133392002712976742718
    static const double kT2 = 0x1B54C91D865AFE.0p-57; // 0.0533812378445670393523
    static const double kT3 = 0x191DF3908C33CE.0p-58; // 0.0245283181166547278873
    static const double kT4 = 0x185DADFCECF44E.0p-61; // 0.00297435743359967304927
    static const double kT5 = 0x1362B9BF971BCD.0p-59; // 0.00946564784943673166728
    const f64 z = x*x;
    // polynomial reduction into independent terms for parallel evaluation
    const f64 r = kT4 + z*kT5;
    const f64 t = kT2 + z*kT3;
    const f64 w = z*z;
    const f64 s = z*x;
    const f64 u = kT0 + z*kT1;
    // add up small terms from lowest degree up for efficiency on non-sequential
    // systems (lower terms tend to be ready earlier.)
    const f64 v = (x + s*u) + (s*w)*(t + w*r);
    return odd ? -1.0/v : v;
}

static inline int rempio2(float x, double &y) {
#if FLT_EVAL_METHOD == 0 || FLT_EVAL_METHOD == 1
    static const double kToInt = 1.5 / DBL_EPSILON;
#elif FLT_EVAL_METHOD == 2
    static const double kToInt = 1.5 / LDBL_EPSILON;
#endif
    static const double kInvPio2 = 6.36619772367581382433e-01;
    static const double kPIO2H = 1.57079631090164184570e+00;
    static const double kPIO2T = 1.58932547735281966916e-08;
    floatShape shape = { x };
    uint32_t ix = shape.asInt & 0x7FFFFFFF;
    if (ix < 0x4DC90FDB) { // |x| ~< 2^28*(pi/2)
        const f64 fn = x*kInvPio2 + kToInt - kToInt;
        const int n = int32_t(fn);
        y = x - fn*kPIO2H - fn*kPIO2T;
        return n;
    }
    // value is far too large, something it pathologically wrong
    assert(0 && "function called with a huge value");
}

static inline float cos(float x) {
    floatShape shape = { x };
    uint32_t ix = shape.asInt;
    const uint32_t sign = ix >> 31;
    ix &= 0x7FFFFFFF;
    if (ix <= 0x3F490FDA) { // |x| ~<= pi/4
        if (ix < 0x39800000) // |x| < 2**-12
            return 1.0f;
        return cosdf(x);
    }
    if (ix <= 0x407B53D1) { // |x| ~<= 5*pi/4
        if (ix > 0x4016CBE3) // |x| ~> 3*pi/4
            return -cosdf(sign ? x+kC2PIO2 : x-kC2PIO2);
        return sindf(sign ? x+kC1PIO2 : kC1PIO2-x);
    }
    if (ix <= 0x40E231D5) { // |x| ~<= 9*pi/4
        if (ix > 0x40AFEDDF) // |x| ~> 7*pi/4
            return cosdf(sign ? x+kC4PIO2 : x-kC4PIO2);
        return sindf(sign ? -x-kC3PIO2 : x-kC3PIO2);
    }
    if (ix >= 0x7F800000) // NaN
        return x-x;
    double y;
    const int n = rempio2(x, y);
    switch (n&3) {
    case 0: return cosdf(y);
    case 1: return sindf(-y);
    case 2: return -cosdf(y);
    }
    return sindf(y);
}

static inline float sin(float x) {
    floatShape shape = { x };
    uint32_t ix = shape.asInt;
    const uint32_t sign = ix >> 31;
    ix &= 0x7FFFFFFF;
    if (ix <= 0x3F490FDA) { // |x| ~<= pi/4
        if (ix < 0x39800000) // |x| < 2**-12
            return x;
        return sindf(x);
    }
    if (ix <= 0x407B53D1) { // |x| ~<= 5*pi/4
        if (ix <= 0x4016CBE3) { // |x| ~<= 3pi/4
            if (sign)
                return -cosdf(x + kC1PIO2);
            return cosdf(x - kC1PIO2);
        }
        return sindf(sign ? -(x + kC2PIO2) : -(x - kC2PIO2));
    }
    if (ix <= 0x40E231D5) { // |x| ~<= 9*pi/4
        if (ix <= 0x40AFEDDF) { // |x| ~<= 7*pi/4
            if (sign)
                return cosdf(x + kC3PIO2);
            return -cosdf(x - kC3PIO2);
        }
        return sindf(sign ? x+kC4PIO2 : x-kC4PIO2);
    }
    if (ix >= 0x7F800000) // NaN
        return x-x;
    double y;
    const int n = rempio2(x, y);
    switch (n&3) {
    case 0: return sindf(y);
    case 1: return cosdf(y);
    case 2: return sindf(-y);
    }
    return -cosdf(y);
}

static inline float tan(float x) {
    floatShape shape = { x };
    uint32_t ix = shape.asInt;
    const uint32_t sign = ix >> 31;
    ix &= 0x7FFFFFFF;
    if (ix < 0x3F490FDa) { // |x| ~<= pi/4
        if (ix < 0x39800000) // |x| < 2**-12
            return x;
        return tandf(x, 0);
    }
    if (ix <= 0x407B53D1) { // |x| ~<= 5*pi/4
        if (ix <= 0x4016CBE3) // |x| ~<= 3pi/4
            return tandf((sign ? x+kC1PIO2 : x-kC1PIO2), true);
        return tandf((sign ? x+kC2PIO2 : x-kC2PIO2), false);
    }
    if (ix <= 0x40E231D5) { // |x| ~<= 9*pi/4
        if (ix <= 0x40AFEDDF) // |x| ~<= 7*pi/4
            return tandf((sign ? x+kC3PIO2 : x-kC3PIO2), true);
        return tandf((sign ? x+kC4PIO2 : x-kC4PIO2), false);
    }
    if (ix >= 0x7F800000) // NaN
        return x-x;
    double y;
    const int n = rempio2(x, y);
    return tandf(y, n&1);
}

static inline void sincos(float x, float &s, float &c) {
    floatShape shape = { x };
    uint32_t ix = shape.asInt;
    const uint32_t sign = ix >> 31;
    ix &= 0x7FFFFFFF;
    if (ix <= 0x3F490FDA) { // |x| ~<= pi/4
        if (ix < 0x39800000) { // |x| < 2**-12
            s = x;
            c = 1.0f;
            return;
        }
        s = sindf(x);
        c = cosdf(x);
        return;
    }
    if (ix <= 0x407B53D1) { // |x| ~<= 5*pi/4
        if (ix <= 0x4016CBE3) { // |x| ~<= 3*pi/4
            if (sign) {
                const double value = x + kC1PIO2;
                s = -cosdf(value);
                c = sindf(value);
            } else {
                const double value = kC1PIO2 - x;
                s = cosdf(value);
                c = sindf(value);
            }
            return;
        }
        const double value = sign ? x+kC2PIO2 : x-kC2PIO2;
        s = -sindf(value);
        c = -cosdf(value);
        return;
    }
    if (ix <= 0x40E231D5) { // |x| ~<= 9*pi/4
        if (ix <= 0x40AFEDDF) { // |x| ~<= 7*pi/4
            if (sign) {
                const double value = x+kC3PIO2;
                s = cosdf(value);
                c = -sindf(value);
            } else {
                const double value = x-kC3PIO2;
                s = -cosdf(value);
                c = sindf(value);
            }
            return;
        }
        const double value = sign ? x+kC4PIO2 : x-kC4PIO2;
        s = sindf(value);
        c = cosdf(value);
        return;
    }
    if (ix >= 0x7F800000) { // NaN
        s = x-x;
        c = x-x;
        return;
    }
    double y;
    const int n = rempio2(x, y);
    const float ss = sindf(y);
    const float cc = cosdf(y);
    switch (n&3) {
    case 0: s = ss, c = cc; break;
    case 1: s = cc, c = -ss; break;
    case 2: s = -ss, c = -cc; break;
    }
    s = -cc;
    c = ss;
}

static inline float floor(float x) {
    floatShape shape = { x };
    const int e = int(shape.asInt >> 23 & 0xFF) - 0x7F;
    if (e >= 23)
        return x;
    if (e >= 0) {
        const uint32_t m = 0x007FFFFF >> e;
        if ((shape.asInt & m) == 0)
            return x;
        if ((shape.asInt >> 31))
            shape.asInt += m;
        shape.asInt &= ~m;
    } else {
        if ((shape.asInt >> 31) == 0)
            shape.asInt = 0;
        else if ((shape.asInt << 1))
            shape.asFloat = -1.0f;
    }
    return shape.asFloat;
}

static inline float ceil(float x) {
    floatShape shape = { x };
    const int e = int(shape.asInt >> 23 & 0xFF) - 0x7F;
    if (e >= 23)
        return x;
    if (e >= 0) {
        const uint32_t m = 0x007FFFFF >> e;
        if ((shape.asInt & m) == 0)
            return x;
        if ((shape.asInt >> 31) == 0)
            shape.asInt += m;
        shape.asInt &= ~m;
    } else {
        if ((shape.asInt >> 31))
            shape.asFloat = -0.0f;
        else if ((shape.asInt << 1))
            shape.asFloat = 1.0f;
    }
    return shape.asFloat;
}

// |(log(1+s)-log(1-s))/s - Lg(s)| < 2**-34.24 (~[-4.95e-11, 4.97e-11])
static inline float log2(float x) {
    static const float kIvLn2Hi = 1.4428710938e+00; // 0x3fb8b000
    static const float kIvLn2Lo = -1.7605285393e-04; // 0xb9389ad4
    static const float kLg1 = 0xAAAAAA.0p-24; // 0.66666662693
    static const float kLg2 = 0xcCCE13.0p-25; // 0.40000972152
    static const float kLg3 = 0x91E9EE.0p-25; // 0.28498786688
    static const float kLg4 = 0xF89E26.0p-26; // 0.24279078841
    floatShape shape = { x };
    uint32_t ix = shape.asInt;
    int k = 0;
    if (ix < 0x00800000 || (ix >> 31)) { // x <2**-16
        if ((ix << 1) == 0)
            return -1/(x*x); // log(+-0) = -inf
        if ((ix >> 31))
            return (x-x)/0.0f; // log(-#) = NaN
        // scale up subnormal number
        k -= 25;
        x *= 0x1p25f;
        shape.asFloat = x;
        ix = shape.asInt;
    } else if (ix >= 0x7F800000) {
        return x;
    } else if (ix == 0x3F800000) {
        return 0.0f;
    }
    // reduce x into [sqrt(2)/2, sqrt(2)]
    ix += 0x3F800000 - 0x3F3504F3;
    k += int(ix>>23) - 0x7f;
    ix = (ix & 0x007FFFFF) + 0x3F3504F3;
    shape.asInt = ix;
    x = shape.asFloat;
    const f32 f = x - 1.0f;
    const f32 s = f/(2.0f + f);
    const f32 z = s*s;
    const f32 w = z*z;
    const f32 t1 = w*(kLg2+w*kLg4);
    const f32 t2 = z*(kLg1+w*kLg3);
    const f32 R = t2 + t1;
    const f32 hfsq = 0.5f*f*f;
    f32 hi = f - hfsq;
    shape.asFloat = hi;
    shape.asInt &= 0xFFFFF000;
    hi = shape.asFloat;
    const f32 lo = f - hi - hfsq + s*(hfsq+R);
    return (lo+hi)*kIvLn2Lo + lo*kIvLn2Hi + hi*kIvLn2Hi + k;
}

enum axis {
    kAxisX,
    kAxisY,
    kAxisZ
};

}

#endif
