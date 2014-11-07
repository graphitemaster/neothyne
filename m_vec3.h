#ifndef M_VEC3_HDR
#define M_VEC3_HDR
#include <math.h>
#include <stddef.h>

#include "m_const.h"

namespace m {

struct vec3 {
    float x;
    float y;
    float z;

    constexpr vec3();
    constexpr vec3(float nx, float ny, float nz);
    constexpr vec3(float a);

    void endianSwap();

    void rotate(float angle, const vec3 &axe);

    float absSquared() const;
    float abs() const;

    void normalize();
    vec3 normalized() const;
    bool isNormalized() const;
    bool isNull() const;

    bool isNullEpsilon(const float epsilon = kEpsilon) const;
    bool equals(const vec3 &cmp, const float epsilon) const;

    void setLength(float scaleLength);
    void maxLength(float length);

    vec3 cross(const vec3 &v) const;

    vec3 &operator +=(const vec3 &vec);
    vec3 &operator -=(const vec3 &vec);
    vec3 &operator *=(float value);
    vec3 &operator /=(float value);
    vec3 operator -() const;
    float operator[](size_t index) const;
    float &operator[](size_t index);

    static inline const vec3 getAxis(axis a) {
        if (a == kAxisX) return xAxis;
        else if (a == kAxisY) return yAxis;
        return zAxis;
    }

    static bool rayCylinderIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &edgeStart, const vec3 &edgeEnd, float radius, float *fraction);

    static bool raySphereIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &sphere, float radius, float *fraction);

    static const vec3 xAxis;
    static const vec3 yAxis;
    static const vec3 zAxis;
    static const vec3 origin;
};

inline constexpr vec3::vec3()
    : x(0.0f)
    , y(0.0f)
    , z(0.0f)
{
}

inline constexpr vec3::vec3(float nx, float ny, float nz)
    : x(nx)
    , y(ny)
    , z(nz)
{
}

inline constexpr vec3::vec3(float a)
    : x(a)
    , y(a)
    , z(a)
{
}

inline float vec3::absSquared() const {
    return x * x + y * y + z * z;
}

inline float vec3::abs() const {
    return sqrtf(x * x + y * y + z * z);
}

inline void vec3::normalize() {
    const float length = 1.0f / abs();
    x *= length;
    y *= length;
    z *= length;
}

inline vec3 vec3::normalized() const {
    const float scale = 1.0f / abs();
    return vec3(x * scale, y * scale, z * scale);
}

inline bool vec3::isNormalized() const {
    return fabsf(abs() - 1.0f) < kEpsilon;
}

inline bool vec3::isNull() const {
    return x == 0.0f && y == 0.0f && z == 0.0f;
}

inline bool vec3::isNullEpsilon(const float epsilon) const {
    return equals(origin, epsilon);
}

inline bool vec3::equals(const vec3 &cmp, const float epsilon) const {
    return (fabsf(x - cmp.x) < epsilon)
        && (fabsf(y - cmp.y) < epsilon)
        && (fabsf(z - cmp.z) < epsilon);
}

inline void vec3::setLength(float scaleLength) {
    const float length = scaleLength / abs();
    x *= length;
    y *= length;
    z *= length;
}

inline void vec3::maxLength(float length) {
    const float currentLength = abs();
    if (currentLength > length) {
        const float scale = length / currentLength;
        x *= scale;
        y *= scale;
        z *= scale;
    }
}

inline vec3 vec3::cross(const vec3 &v) const {
    return vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
}

inline vec3 &vec3::operator +=(const vec3 &vec) {
    x += vec.x;
    y += vec.y;
    z += vec.z;
    return *this;
}

inline vec3 &vec3::operator -=(const vec3 &vec) {
    x -= vec.x;
    y -= vec.y;
    z -= vec.z;
    return *this;
}

inline vec3 &vec3::operator *=(float value) {
    x *= value;
    y *= value;
    z *= value;
    return *this;
}

inline vec3 &vec3::operator /=(float value) {
    const float inv = 1.0f / value;
    x *= inv;
    y *= inv;
    z *= inv;
    return *this;
}

inline vec3 vec3::operator -() const {
    return vec3(-x, -y, -z);
}

inline float vec3::operator[](size_t index) const {
    return (&x)[index];
}

inline float &vec3::operator[](size_t index) {
    return (&x)[index];
}

inline vec3 operator+(const vec3 &a, const vec3 &b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

inline vec3 operator-(const vec3 &a, const vec3 &b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

inline vec3 operator*(const vec3 &a, float value) {
    return vec3(a.x * value, a.y * value, a.z * value);
}

inline vec3 operator*(float value, const vec3 &a) {
    return vec3(a.x * value, a.y * value, a.z * value);
}

inline vec3 operator/(const vec3 &a, float value) {
    const float inv = 1.0f / value;
    return vec3(a.x * inv, a.y * inv, a.z * inv);
}

inline vec3 operator^(const vec3 &a, const vec3 &b) {
    return vec3((a.y * b.z - a.z * b.y),
                (a.z * b.x - a.x * b.z),
                (a.x * b.y - a.y * b.x));
}

inline float operator*(const vec3 &a, const vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline bool operator==(const vec3 &a, const vec3 &b) {
    return (fabs(a.x - b.x) < kEpsilon)
        && (fabs(a.y - b.y) < kEpsilon)
        && (fabs(a.z - b.z) < kEpsilon);
}

inline bool operator!=(const vec3 &a, const vec3 &b) {
    return (fabsf(a.x - b.x) > kEpsilon)
        || (fabsf(a.y - b.y) > kEpsilon)
        || (fabsf(a.z - b.z) > kEpsilon);
}

}

#endif
