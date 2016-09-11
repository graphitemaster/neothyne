#ifndef M_VEC3_HDR
#define M_VEC3_HDR
#include <stddef.h>

#include "m_const.h"

#include "u_algorithm.h"

#ifdef __SSE2__
#include <xmmintrin.h>

// When both arguments to _mm_shuffle_ps are the same we can achieve the
// same result using pshufd instruction instead. This saves ~2 mov instructions
// on average and has less latency; similarly, multiple pshufd instructions
// can be scheduled in parallel and complete together.
#define _mm_pshufd(xmm, mask) \
    _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(xmm), mask))
#endif

namespace m {

struct vec2 {
    union {
        struct { float x, y; };
        float f[2];
    };

    constexpr vec2();
    constexpr vec2(const float (&vals)[2]);
    constexpr vec2(float nx, float ny);
    constexpr vec2(float a);
    float operator[](size_t index) const;
    float &operator[](size_t index);

    void endianSwap();
};

inline constexpr vec2::vec2()
    : x(0.0f)
    , y(0.0f)
{
}

inline constexpr vec2::vec2(const float (&vals)[2])
    : x(vals[0])
    , y(vals[1])
{
}

inline constexpr vec2::vec2(float nx, float ny)
    : x(nx)
    , y(ny)
{
}

inline constexpr vec2::vec2(float a)
    : x(a)
    , y(a)
{
}

inline float vec2::operator[](size_t index) const {
    return f[index];
}

inline float &vec2::operator[](size_t index) {
    return f[index];
}

vec2 sincos(float x);

struct vec3 {
    union {
        struct { float x, y, z; };
        float f[3];
    };

    constexpr vec3();
    constexpr vec3(const float (&vals)[3]);
    constexpr vec3(float nx, float ny, float nz);
    constexpr vec3(float a);

    void endianSwap();

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

    vec3 &operator+=(const vec3 &vec);
    vec3 &operator-=(const vec3 &vec);
    vec3 &operator*=(const vec3 &vec);
    vec3 &operator*=(float value);
    vec3 &operator/=(float value);

    vec3 operator-() const;
    float operator[](size_t index) const;
    float &operator[](size_t index);

    static inline const vec3 getAxis(axis a) {
        return kAxis[a];
    }

    static bool rayCylinderIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &edgeStart, const vec3 &edgeEnd, float radius, float *fraction);

    static bool raySphereIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &sphere, float radius, float *fraction);

    static vec3 min(const vec3 &lhs, const vec3 &rhs);
    static vec3 max(const vec3 &lhs, const vec3 &rhs);

    static vec3 rand(float mx, float my, float mz);

    static const vec3 kAxis[3];
    static const vec3 origin;

    static const vec3 &xAxis;// = kAxis[0];
    static const vec3 &yAxis;// = kAxis[1];
    static const vec3 &zAxis;// = kAxis[2];
};

inline constexpr vec3::vec3()
    : x(0.0f)
    , y(0.0f)
    , z(0.0f)
{
}

inline constexpr vec3::vec3(const float (&vals)[3])
    : x(vals[0])
    , y(vals[1])
    , z(vals[2])
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

inline void vec3::normalize() {
    const float length = 1.0f / abs();
    x *= length;
    y *= length;
    z *= length;
}

inline vec3 vec3::normalized() const {
    const float scale = 1.0f / abs();
    return { x * scale, y * scale, z * scale };
}

inline bool vec3::isNormalized() const {
    return m::abs(abs() - 1.0f) < kEpsilon;
}

inline bool vec3::isNull() const {
    return x == 0.0f && y == 0.0f && z == 0.0f;
}

inline bool vec3::isNullEpsilon(const float epsilon) const {
    return equals(origin, epsilon);
}

inline bool vec3::equals(const vec3 &cmp, const float epsilon) const {
    return (m::abs(x - cmp.x) < epsilon)
        && (m::abs(y - cmp.y) < epsilon)
        && (m::abs(z - cmp.z) < epsilon);
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
    return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
}

inline vec3 &vec3::operator+=(const vec3 &vec) {
    x += vec.x;
    y += vec.y;
    z += vec.z;
    return *this;
}

inline vec3 &vec3::operator-=(const vec3 &vec) {
    x -= vec.x;
    y -= vec.y;
    z -= vec.z;
    return *this;
}

inline vec3 &vec3::operator*=(const vec3 &vec) {
    x *= vec.x;
    y *= vec.y;
    z *= vec.z;
    return *this;
}

inline vec3 &vec3::operator*=(float value) {
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
    return { -x, -y, -z };
}

inline float vec3::operator[](size_t index) const {
    return f[index];
}

inline float &vec3::operator[](size_t index) {
    return f[index];
}

inline vec3 operator+(const vec3 &a, const vec3 &b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

inline vec3 operator-(const vec3 &a, const vec3 &b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

inline vec3 operator*(const vec3 &a, float value) {
    return { a.x * value, a.y * value, a.z * value };
}

inline vec3 operator*(float value, const vec3 &a) {
    return { a.x * value, a.y * value, a.z * value };
}

inline vec3 operator/(const vec3 &a, float value) {
    const float inv = 1.0f / value;
    return { a.x * inv, a.y * inv, a.z * inv };
}

inline float operator*(const vec3 &a, const vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline bool operator==(const vec3 &a, const vec3 &b) {
    return (m::abs(a.x - b.x) < kEpsilon)
        && (m::abs(a.y - b.y) < kEpsilon)
        && (m::abs(a.z - b.z) < kEpsilon);
}

inline bool operator!=(const vec3 &a, const vec3 &b) {
    return (m::abs(a.x - b.x) > kEpsilon)
        || (m::abs(a.y - b.y) > kEpsilon)
        || (m::abs(a.z - b.z) > kEpsilon);
}

inline vec3 vec3::min(const vec3 &lhs, const vec3 &rhs) {
    return { u::min(lhs.x, rhs.x), u::min(lhs.y, rhs.y), u::min(lhs.z, rhs.z) };
}

inline vec3 vec3::max(const vec3 &lhs, const vec3 &rhs) {
    return { u::max(lhs.x, rhs.x), u::max(lhs.y, rhs.y), u::max(lhs.z, rhs.z) };
}

inline vec3 clamp(const vec3 &current, const vec3 &min, const vec3 &max) {
    return { clamp(current.x, min.x, max.x),
             clamp(current.y, min.y, max.y),
             clamp(current.z, min.z, max.z) };
}

struct vec4 {
    union {
        struct { float x, y, z, w; };
        float f[4];
        vec3 asVec3;
#ifdef __SSE2__
        __m128 v;
#endif
    };

    constexpr vec4();
    constexpr vec4(float n);
    constexpr vec4(const float (&vals)[4]);
    constexpr vec4(const vec3 &vec, float w);
    constexpr vec4(float x, float y, float z, float w);

    float &operator[](size_t index);
    const float &operator[](size_t index) const;
    vec4 addw(float f) const;

    vec4 &operator*=(float k);
    vec4 &operator+=(const vec4 &o);
    vec4 operator-() const;

    float abs() const;

    void endianSwap();

    static float dot(const vec4 &l, const vec4 &r);

    friend vec4 operator*(const vec4 &l, const vec4 &r);
    friend vec4 operator*(const vec4 &l, float k);
    friend vec4 operator+(const vec4 &l, const vec4 &r);
    friend vec4 operator-(const vec4 &l, const vec4 &r);

#ifdef __SSE2__
    constexpr vec4(__m128 v);
#endif

    template <size_t X, size_t Y, size_t Z, size_t W>
    vec4 swizzle() const;
    template <size_t N>
    vec4 splat() const;
};

inline constexpr vec4::vec4()
    : x(0.0f)
    , y(0.0f)
    , z(0.0f)
    , w(1.0f)
{
}

inline constexpr vec4::vec4(float n)
    : x(n)
    , y(n)
    , z(n)
    , w(n)
{
}

inline constexpr vec4::vec4(const float (&vals)[4])
    : x(vals[0])
    , y(vals[1])
    , z(vals[2])
    , w(vals[3])
{
}

inline constexpr vec4::vec4(const vec3 &vec, float w)
    : x(vec.x)
    , y(vec.y)
    , z(vec.z)
    , w(w)
{
}

inline constexpr vec4::vec4(float x, float y, float z, float w)
    : x(x)
    , y(y)
    , z(z)
    , w(w)
{
}

inline float &vec4::operator[](size_t index) {
    return f[index];
}

inline const float &vec4::operator[](size_t index) const {
    return f[index];
}

template <size_t N>
inline vec4 vec4::splat() const {
    return swizzle<N,N,N,N>();
}

#ifdef __SSE2__
inline constexpr vec4::vec4(__m128 v)
    : v(v)
{
}

inline vec4 &vec4::operator*=(float k) {
    v = _mm_mul_ps(v, _mm_set1_ps(k));
    return *this;
}

inline vec4 &vec4::operator+=(const vec4 &o) {
    v = _mm_add_ps(v, o.v);
    return *this;
}

inline vec4 vec4::operator-() const {
    alignas(16) static const float kNegZero[] = { -0.0f, -0.0f, -0.0f, -0.0f };
    return _mm_xor_ps(v, *(const __m128 *)kNegZero);
}

inline float vec4::dot(const vec4 &l, const vec4 &r) {
    __m128 e = _mm_mul_ps(l.v, r.v);
    e = _mm_add_ps(e, _mm_pshufd(e, _MM_SHUFFLE(0,1,3,2)));
    return _mm_cvtss_f32(_mm_add_ss(e, _mm_pshufd(e, _MM_SHUFFLE(0,1,0,1))));
}

inline vec4 operator*(const vec4 &l, const vec4 &r) {
    return _mm_mul_ps(l.v, r.v);
}

inline vec4 operator*(const vec4 &l, float k) {
    return _mm_mul_ps(l.v, _mm_set1_ps(k));
}

inline vec4 operator+(const vec4 &l, const vec4 &r) {
    return _mm_add_ps(l.v, r.v);
}

inline vec4 operator-(const vec4 &l, const vec4 &r) {
    return _mm_sub_ps(l.v, r.v);
}

inline vec4 operator^(const vec4 &l, const vec4 &r) {
    __m128 a_yzx = _mm_shuffle_ps(l.v, l.v, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_yzx = _mm_shuffle_ps(r.v, r.v, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 c = _mm_sub_ps(_mm_mul_ps(l.v, b_yzx), _mm_mul_ps(a_yzx, r.v));
    return _mm_shuffle_ps(c, c, _MM_SHUFFLE(3, 0, 2, 1));
}

inline vec4 vec4::addw(float f) const {
    return _mm_add_ps(v, _mm_set_ps(f, 0, 0, 0));
}

template <size_t X, size_t Y, size_t Z, size_t W>
inline vec4 vec4::swizzle() const {
    return _mm_pshufd(v, _MM_SHUFFLE(W, Z, Y, X));
}

#else
inline vec4 &vec4::operator*=(float k) {
    x *= k;
    y *= k;
    z *= k;
    w *= k;
    return *this;
}

inline vec4 &vec4::operator+=(const vec4 &o) {
    x += o.x;
    y += o.y;
    z += o.z;
    w += o.w;
    return *this;
}

inline vec4 vec4::operator-() const {
    return { -x, -y, -z, -w };
}

inline float vec4::dot(const vec4 &l, const vec4 &r) {
    return l.x*r.x + l.y*r.y + l.z*r.z + l.w*r.w;
}

inline vec4 operator*(const vec4 &l, const vec4 &r) {
    return { l.x * r.x, l.y * r.y, l.z * r.z, l.w * r.w };
}

inline vec4 operator*(const vec4 &l, float k) {
    return { l.x*k, l.y*k, l.z*k, l.w*k };
}

inline vec4 operator+(const vec4 &l, const vec4 &r) {
    return { l.x+r.x, l.y+r.y, l.z+r.z, l.w+r.w };
}

inline vec4 operator-(const vec4 &l, const vec4 &r) {
    return { l.x-r.x, l.y-r.y, l.z-r.z, l.w-r.w };
}

inline vec4 operator^(const vec4 &l, const vec4 &r) {
    return { l.y*r.z - l.z*r.y,
             l.z*r.x - l.x*r.z,
             l.x*r.y - l.y*r.x,
             0.0f };
}

inline vec4 vec4::addw(float f) const {
    return { x, y, z, w + f };
}

template <size_t X, size_t Y, size_t Z, size_t W>
inline vec4 vec4::swizzle() const {
    return { f[X], f[Y], f[Z], f[W] };
}

#endif

inline vec3 operator^(const vec3 &l, const vec3 &r) {
    const vec4 a { l, 0.0f };
    const vec4 b { r, 0.0f };
    const vec4 v = a ^ b;
    return { v.x, v.y, v.z };
}

}

#endif
