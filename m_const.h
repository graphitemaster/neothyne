#ifndef M_CONST_HDR
#define M_CONST_HDR
#include <stdint.h>

namespace m {

const float kPi        = 3.1415926535f;
const float kTau       = kPi * 2.0f;
const float kPiHalf    = kPi * 0.5f;
const float kSqrt2Half = 0.707106781186547f;
const float kSqrt2     = 1.4142135623730950488f;
const float kEpsilon   = 0.00001f;
const float kDegToRad  = kPi / 180.0f;
const float kRadToDeg  = 180.0f / kPi;

inline float toRadian(float x) {
    return x * kDegToRad;
}

inline float toDegree(float x) {
    return x * kRadToDeg;
}

inline float angleMod(float angle) {
    static const float kF = 65536.0f / 360.0f;
    static const float kI = 360.0f / 65536.0f;
    return kI * ((int)(angle * kF) & 65535);
}

template <typename T>
inline T clamp(const T& current, const T &min, const T &max) {
    return (current > max) ? max : ((current < min) ? min : current);
}

union floatShape {
    floatShape(floatShape&) = delete;
    floatShape(floatShape&&) = delete;

    constexpr floatShape(float value)
        : asFloat(value)
    {
    }

    constexpr floatShape(uint32_t value)
        : asInt(value)
    {
    }

    float asFloat;
    uint32_t asInt;
};

inline float abs(float v) {
    floatShape s = { v };
    s.asInt &= 0x7FFFFFFF;
    return s.asFloat;
}

enum axis {
    kAxisX,
    kAxisY,
    kAxisZ
};

}

#endif
