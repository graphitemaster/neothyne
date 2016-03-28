#include "m_mat.h"
#include "m_vec.h"
#include "m_quat.h"

#ifdef __SSE2__
#include <xmmintrin.h>
#endif

namespace m {

///! mat4x4
mat4 mat4::identity() {
    return { { 1.0f, 0.0f, 0.0f, 0.0f },
             { 0.0f, 1.0f, 0.0f, 0.0f },
             { 0.0f, 0.0f, 1.0f, 0.0f },
             { 0.0f, 0.0f, 0.0f, 1.0f } };
}

mat4 mat4::scale(const vec3 &s) {
    return { { s.x,  0.0f, 0.0f, 0.0f },
             { 0.0f, s.y,  0.0f, 0.0f },
             { 0.0f, 0.0f, s.z,  0.0f },
             { 0.0f, 0.0f, 0.0f, 1.0f } };
}

mat4 mat4::rotate(const vec3 &r) {
    const m::vec2 x = m::sincos(toRadian(r.x));
    const m::vec2 y = m::sincos(toRadian(r.y));
    const m::vec2 z = m::sincos(toRadian(r.z));

    const m::mat4 rx = { { 1.0f, 0.0f, 0.0f, 0.0f },
                         { 0.0f, x.y, -x.x, 0.0f },
                         { 0.0f, x.x, x.y, 0.0f },
                         { 0.0f, 0.0f, 0.0f, 1.0f } },
                  ry = { { y.y, 0.0f, -y.x, 0.0f },
                         { 0.0f, 1.0f, 0.0f, 0.0f },
                         { y.x, 0.0f, y.y, 0.0f },
                         { 0.0f, 0.0f, 0.0f, 1.0f } },
                  rz = { { z.y, -z.x, 0.0f, 0.0f },
                         { z.x, z.y, 0.0f, 0.0f },
                         { 0.0f, 0.0f, 1.0f, 0.0f },
                         { 0.0f, 0.0f, 0.0f, 1.0f } };

    return rz * ry * rx;
}

mat4 mat4::translate(const vec3 &t) {
    return { { 1.0f, 0.0f, 0.0f, t.x },
             { 0.0f, 1.0f, 0.0f, t.y },
             { 0.0f, 0.0f, 1.0f, t.z },
             { 0.0f, 0.0f, 0.0f, 1.0f } };
}

mat4 mat4::lookat(const vec3 &target, const vec3 &up) {
    const vec3 n = target.normalized(),
               u = up.normalized().cross(n),
               v = n.cross(u);
    return { { u.x,  u.y,  u.z,  0.0f },
             { v.x,  v.y,  v.z,  0.0f },
             { n.x,  n.y,  n.z,  0.0f },
             { 0.0f, 0.0f, 0.0f, 1.0f } };
}

mat4 mat4::lookat(const vec3 &p, const quat &q) {
    mat4 m;
    m.a = { 1.0f - 2.0f * (q.y * q.y + q.z*q.z),
            2.0f * (q.x * q.y - q.w * q.z),
            2.0f * (q.x * q.z + q.w * q.y),
            0.0f };
    m.b = { 2.0f * (q.x * q.y + q.w * q.z),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z),
            2.0f * (q.y * q.z - q.w * q.x),
            0.0f };
    m.c = { 2.0f * (q.x * q.z - q.w * q.y),
            2.0f * (q.y * q.z + q.w * q.x),
            1.0f - 2.0f * (q.x * q.x + q.y * q.y),
            0.0f };
    m.d = { -(p.x * m.a.x + p.y * m.b.x + p.z * m.c.x),
            -(p.x * m.a.y + p.y * m.b.y + p.z * m.c.y),
            -(p.x * m.a.z + p.y * m.b.z + p.z * m.c.z),
            1.0f };
    return m;
}

mat4 mat4::project(const m::perspective &p) {
    const float ar = p.width / p.height;
    const float range = p.nearp - p.farp;
    const float halfFov = m::tan(m::toRadian(p.fov) * 0.5f);
    return { { 1.0f / (halfFov * ar), 0.0f, 0.0f, 0.0f },
             { 0.0f, 1.0f / halfFov, 0.0f, 0.0f },
             { 0.0f, 0.0f, (-p.nearp - p.farp) / range, 2.0f * p.farp * p.nearp / range },
             { 0.0f, 0.0f, 1.0f, 0.0f } };
}

mat4 mat4::project(float angle, float nearClip, float farClip, float bias) {
    const float halfFOV = 1.0f / m::tan(m::toRadian(angle) * 0.5f);
    const float zRange = nearClip - farClip;
    return { { halfFOV, 0.0f, 0.0f, 0.0f },
             { 0.0f, halfFOV, 0.0f, 0.0f },
             { 0.0f, 0.0f, -(nearClip + farClip) / zRange, 2.0f * nearClip * farClip / zRange + bias },
             { 0.0f, 0.0f, 1.0f, 0.0f} };
}

void mat4::getOrient(vec3 *direction, vec3 *up, vec3 *side) const {
    if (side) {
        side->x = a[0];
        side->y = b[0];
        side->z = c[0];
    }
    if (up) {
        up->x = a[1];
        up->y = b[1];
        up->z = c[1];
    }
    if (direction) {
        direction->x = a[2];
        direction->y = b[2];
        direction->z = c[2];
    }
}

mat4 mat4::operator*(const mat4 &t) const {
    return { t.a*a.splat<0>() + t.b*a.splat<1>() + t.c*a.splat<2>() + t.d*a.splat<3>(),
             t.a*b.splat<0>() + t.b*b.splat<1>() + t.c*b.splat<2>() + t.d*b.splat<3>(),
             t.a*c.splat<0>() + t.b*c.splat<1>() + t.c*c.splat<2>() + t.d*c.splat<3>(),
             t.a*d.splat<0>() + t.b*d.splat<1>() + t.c*d.splat<2>() + t.d*d.splat<3>() };
}

#ifdef __SSE2__
inline m::vec4 mat4::mul2x2(const m::vec4 &l, const m::vec4 &r) {
    return r.swizzle<0,0,2,2>() * _mm_movelh_ps(l.v, l.v) +
           r.swizzle<1,1,3,3>() * _mm_movehl_ps(l.v, l.v);
}

inline m::vec4 mat4::det2x2(const m::vec4 &v) {
    const m::vec4 &t = v.swizzle<0,0,1,1>() * v.swizzle<3,3,2,2>();
    return _mm_sub_ps(_mm_unpacklo_ps(t.v, t.v),
                      _mm_unpackhi_ps(t.v, t.v));
}
inline m::vec4 mat4::adj2x2(const m::vec4 &v) {
    const m::vec4 znzn(0.0f, -0.0f, -0.0f, 0.0f);
    return _mm_xor_ps(v.swizzle<3,1,2,0>().v, znzn.v);
}
inline m::vec4 mat4::inv2x2(const m::vec4 &v) {
    return adj2x2(v) * det2x2(v).rcp();
}
mat4 mat4::inverse() const {
    m::vec4 c0 = a;
    m::vec4 c1 = b;
    m::vec4 c2 = c;
    m::vec4 c3 = d;

    // 2x2 sub matrices
    const m::vec4 &m11 = _mm_movelh_ps(c0.v, c1.v);
    const m::vec4 &m21 = _mm_movehl_ps(c1.v, c0.v);
    const m::vec4 &m12 = _mm_movelh_ps(c2.v, c3.v);
    const m::vec4 &m22 = _mm_movehl_ps(c3.v, c2.v);
    // inverse sub matrices
    const m::vec4 &inv11 = inv2x2(m11);
    const m::vec4 &inv22 = inv2x2(m22);
    // partitions
    const m::vec4 &p11 = inv2x2(m11 - mul2x2(mul2x2(m12, inv22), m21));
    const m::vec4 &p21 = mul2x2(mul2x2(-inv22, m21), p11);
    const m::vec4 &p22 = inv2x2(m22 - mul2x2(mul2x2(m21, inv11), m12));
    const m::vec4 &p12 = mul2x2(mul2x2(-inv11, m12), p22);
    return { _mm_movelh_ps(p11.v, p21.v),
             _mm_movehl_ps(p21.v, p11.v),
             _mm_movelh_ps(p12.v, p22.v),
             _mm_movehl_ps(p22.v, p12.v) };

}
#else
inline float mat4::det2x2(float a, float b, float c, float d) {
    return a*d - b*c;
}

inline float mat4::det3x3(float a1, float a2, float a3,
                          float b1, float b2, float b3,
                          float c1, float c2, float c3)
{
    return a1 * det2x2(b2, b3, c2, c3)
         - b1 * det2x2(a2, a3, c2, c3)
         + c1 * det2x2(a2, a3, b2, b3);
}

mat4 mat4::inverse() const {
    mat4 m;
    const float a1 = a.x, a2 = a.y, a3 = a.z, a4 = a.w,
                b1 = b.x, b2 = b.y, b3 = b.z, b4 = b.w,
                c1 = c.x, c2 = c.y, c3 = c.z, c4 = c.w,
                d1 = d.x, d2 = d.y, d3 = d.z, d4 = d.w,
                v1 = det3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4),
                v2 = -det3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4),
                v3 = det3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4),
                v4 = -det3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4),
                nd = a1*v1 + b1*v2 + c1*v3 + d1*v4,
                id = 1.0f / nd;
    m.a.x = v1 * id;
    m.a.y = v2 * id;
    m.a.z = v3 * id;
    m.a.w = v4 * id;
    m.b.x = -det3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4) * id;
    m.b.y = det3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4) * id;
    m.b.z = -det3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4) * id;
    m.b.w = det3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4) * id;
    m.c.x = det3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4) * id;
    m.c.y = -det3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4) * id;
    m.c.z = det3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4) * id;
    m.c.w = -det3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4) * id;
    m.d.x = -det3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3) * id;
    m.d.y = det3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3) * id;
    m.d.z = -det3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3) * id;
    m.d.w = det3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3) * id;
    return m;
}
#endif

///! mat3x3
void mat3x3::convertQuaternion(const quat &q) {
    const float x = q.x;
    const float y = q.y;
    const float z = q.z;
    const float w = q.w;
    const float tx = 2*x;
    const float ty = 2*y;
    const float tz = 2*z;
    const float txx = tx*x;
    const float tyy = ty*y;
    const float tzz = tz*z;
    const float txy = tx*y;
    const float txz = tx*z;
    const float tyz = ty*z;
    const float twx = w*tx;
    const float twy = w*ty;
    const float twz = w*tz;
    a = { 1 - (tyy + tzz), txy - twz, txz + twy };
    b = { txy + twz, 1 - (txx + tzz), tyz - twx };
    c = { txz - twy, tyz + twx, 1 - (txx + tyy) };
}

///! mat3x4
void mat3x4::invert(const mat3x4 &o) {
    mat3x3 inv(vec3(o.a.x, o.b.x, o.c.x),
               vec3(o.a.y, o.b.y, o.c.y),
               vec3(o.a.z, o.b.z, o.c.z));
    inv.a /= (inv.a * inv.a);
    inv.b /= (inv.b * inv.b);
    inv.c /= (inv.c * inv.c);
    const vec3 trans(o.a.w, o.b.w, o.c.w);
    a = vec4(inv.a, -inv.a*trans);
    b = vec4(inv.b, -inv.b*trans);
    c = vec4(inv.c, -inv.c*trans);
}

}
