#ifndef M_MAT_HDR
#define M_MAT_HDR
#include "m_vec.h"

namespace m {

struct quat;

struct perspective {
    perspective();
    float fov;
    float width;
    float height;
    float nearp;
    float farp;
};

inline perspective::perspective()
    : fov(0.0f)
    , width(0.0f)
    , height(0.0f)
    , nearp(0.0f)
    , farp(0.0f)
{
}

///! mat4x4
struct mat4 {
    vec4 a, b, c, d;
    constexpr mat4() = default;
    constexpr mat4(const vec4 &a, const vec4 &b, const vec4 &c, const vec4 &d);

    static mat4 identity();
    mat4 inverse();
    mat4 operator*(const mat4 &t) const;

    static mat4 project(float angle, float nearClip, float farClip);
    static mat4 project(const perspective &p);
    static mat4 scale(const vec3 &s);
    static mat4 rotate(const vec3 &r);
    static mat4 translate(const vec3 &t);
    static mat4 lookat(const vec3 &target, const vec3 &up);
    static mat4 lookat(const vec3 &position, const quat &q);

    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;

    float *ptr();
    const float *ptr() const;

private:
    static float det2x2(float a, float b, float c, float d);
    static float det3x3(float a1, float a2, float a3,
                        float b1, float b2, float b3,
                        float c1, float c2, float c3);
};

inline float *mat4::ptr() {
    return &a.x;
}

inline const float *mat4::ptr() const {
    return &a.x;
}

///! mat3x3
struct mat3x3 {
    vec3 a, b, c;

    mat3x3();
    mat3x3(const vec3 &a, const vec3 &b, const vec3 &c);

    explicit mat3x3(const quat &q);
    explicit mat3x3(const quat &q, const vec3 &scale);

protected:
    void convertQuaternion(const quat &q);
};

inline mat3x3::mat3x3() = default;

inline mat3x3::mat3x3(const vec3 &a, const vec3 &b, const vec3 &c)
    : a(a)
    , b(b)
    , c(c)
{
}

inline mat3x3::mat3x3(const quat &q) {
    convertQuaternion(q);
}

inline mat3x3::mat3x3(const quat &q, const vec3 &scale) {
    convertQuaternion(q);
    a *= scale;
    b *= scale;
    c *= scale;
}

///! mat3x4
struct mat3x4 {
    vec4 a, b, c;

    mat3x4();
    mat3x4(const vec4 &a, const vec4 &b, const vec4 &c);
    explicit mat3x4(const mat3x3 &rot, const vec3 &trans);
    explicit mat3x4(const quat &rot, const vec3 &trans);
    explicit mat3x4(const quat &rot, const vec3 &trans, const vec3 &scale);

    void invert(const mat3x4 &o);
    mat3x4 &operator*=(const mat3x4 &o);
    mat3x4 &operator+=(const mat3x4 &o);
    mat3x4 &operator*=(float k);

    friend mat3x4 operator*(const mat3x4 &l, const mat3x4 &r);
    friend mat3x4 operator+(const mat3x4 &l, const mat3x4 &r);
    friend mat3x4 operator*(const mat3x4 &l, float k);
};

inline mat3x4::mat3x4() = default;

inline mat3x4::mat3x4(const vec4 &a, const vec4 &b, const vec4 &c)
    : a(a)
    , b(b)
    , c(c)
{
}

inline mat3x4::mat3x4(const mat3x3 &rot, const vec3 &trans)
    : a(vec4(rot.a, trans.x))
    , b(vec4(rot.b, trans.y))
    , c(vec4(rot.c, trans.z))
{
}

inline mat3x4::mat3x4(const quat &rot, const vec3 &trans)
    : mat3x4(mat3x3(rot), trans)
{
}

inline mat3x4::mat3x4(const quat &rot, const vec3 &trans, const vec3 &scale)
    : mat3x4(mat3x3(rot, scale), trans)
{
}

inline mat3x4 &mat3x4::operator*=(const mat3x4 &o) {
    return (*this = *this * o);
}

inline mat3x4 &mat3x4::operator*=(float k) {
    a *= k;
    b *= k;
    c *= k;
    return *this;
}

inline mat3x4 &mat3x4::operator+=(const mat3x4 &o) {
    a += o.a;
    b += o.b;
    c += o.c;
    return *this;
}

inline mat3x4 operator*(const mat3x4 &l, const mat3x4 &r) {
    return { (r.a*l.a.x + r.b*l.a.y + r.c*l.a.z).addw(l.a.w),
             (r.a*l.b.x + r.b*l.b.y + r.c*l.b.z).addw(l.b.w),
             (r.a*l.c.x + r.b*l.c.y + r.c*l.c.z).addw(l.c.w) };
}

inline mat3x4 operator+(const mat3x4 &l, const mat3x4 &r) {
    return (mat3x4(l) += r);
}

inline mat3x4 operator*(const mat3x4 &l, float k) {
    return (mat3x4(l) *= k);
}

}

#endif
