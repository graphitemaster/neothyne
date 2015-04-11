#include "m_mat.h"
#include "m_vec.h"
#include "m_quat.h"

namespace m {

void mat4::loadIdentity() {
    m[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
    m[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
    m[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
    m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

mat4 mat4::operator*(const mat4 &t) const {
    mat4 r;
    r.m[0] = { m[0].x * t.m[0].x + m[0].y * t.m[1].x + m[0].z * t.m[2].x + m[0].w * t.m[3].x,
               m[0].x * t.m[0].y + m[0].y * t.m[1].y + m[0].z * t.m[2].y + m[0].w * t.m[3].y,
               m[0].x * t.m[0].z + m[0].y * t.m[1].z + m[0].z * t.m[2].z + m[0].w * t.m[3].z,
               m[0].x * t.m[0].w + m[0].y * t.m[1].w + m[0].z * t.m[2].w + m[0].w * t.m[3].w };
    r.m[1] = { m[1].x * t.m[0].x + m[1].y * t.m[1].x + m[1].z * t.m[2].x + m[1].w * t.m[3].x,
               m[1].x * t.m[0].y + m[1].y * t.m[1].y + m[1].z * t.m[2].y + m[1].w * t.m[3].y,
               m[1].x * t.m[0].z + m[1].y * t.m[1].z + m[1].z * t.m[2].z + m[1].w * t.m[3].z,
               m[1].x * t.m[0].w + m[1].y * t.m[1].w + m[1].z * t.m[2].w + m[1].w * t.m[3].w };
    r.m[2] = { m[2].x * t.m[0].x + m[2].y * t.m[1].x + m[2].z * t.m[2].x + m[2].w * t.m[3].x,
               m[2].x * t.m[0].y + m[2].y * t.m[1].y + m[2].z * t.m[2].y + m[2].w * t.m[3].y,
               m[2].x * t.m[0].z + m[2].y * t.m[1].z + m[2].z * t.m[2].z + m[2].w * t.m[3].z,
               m[2].x * t.m[0].w + m[2].y * t.m[1].w + m[2].z * t.m[2].w + m[2].w * t.m[3].w };
    r.m[3] = { m[3].x * t.m[0].x + m[3].y * t.m[1].x + m[3].z * t.m[2].x + m[3].w * t.m[3].x,
               m[3].x * t.m[0].y + m[3].y * t.m[1].y + m[3].z * t.m[2].y + m[3].w * t.m[3].y,
               m[3].x * t.m[0].z + m[3].y * t.m[1].z + m[3].z * t.m[2].z + m[3].w * t.m[3].z,
               m[3].x * t.m[0].w + m[3].y * t.m[1].w + m[3].z * t.m[2].w + m[3].w * t.m[3].w };
    return r;
}

void mat4::setScaleTrans(float sx, float sy, float sz) {
    m[0] = { sx,   0.0f, 0.0f, 0.0f };
    m[1] = { 0.0f, sy,   0.0f, 0.0f };
    m[2] = { 0.0f, 0.0f, sz,   0.0f };
    m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

void mat4::setRotateTrans(float rotateX, float rotateY, float rotateZ) {
    const float x = toRadian(rotateX);
    const float y = toRadian(rotateY);
    const float z = toRadian(rotateZ);

    mat4 rx, ry, rz;
    rx.m[0] = { 1.0f, 0.0f, 0.0f, 0.0f };
    rx.m[1] = { 0.0f, cosf(x), -sinf(x), 0.0f };
    rx.m[2] = { 0.0f, sinf(x), cosf(x), 0.0f };
    rx.m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ry.m[0] = { cosf(y), 0.0f, -sinf(y), 0.0f };
    ry.m[1] = { 0.0f, 1.0f, 0.0f, 0.0f };
    ry.m[2] = { sinf(y), 0.0f, cosf(y), 0.0f };
    ry.m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
    rz.m[0] = { cosf(z), -sinf(z), 0.0f, 0.0f };
    rz.m[1] = { sinf(z), cosf(z), 0.0f, 0.0f };
    rz.m[2] = { 0.0f, 0.0f, 1.0f, 0.0f };
    rz.m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };

    *this = rz * ry * rx;
}

void mat4::setTranslateTrans(float x, float y, float z) {
    m[0] = { 1.0f, 0.0f, 0.0f, x };
    m[1] = { 0.0f, 1.0f, 0.0f, y };
    m[2] = { 0.0f, 0.0f, 1.0f, z };
    m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

void mat4::setCameraTrans(const vec3 &target, const vec3 &up) {
    vec3 N = target.normalized();
    vec3 U = up.normalized().cross(N);
    vec3 V = N.cross(U);
    m[0] = { U.x,  U.y,  U.z,  0.0f };
    m[1] = { V.x,  V.y,  V.z,  0.0f };
    m[2] = { N.x,  N.y,  N.z,  0.0f };
    m[3] = { 0.0f, 0.0f, 0.0f, 1.0f };
}

void mat4::setCameraTrans(const vec3 &p, const quat &q) {
    m[0] = { 1.0f - 2.0f * (q.y * q.y + q.z*q.z),
             2.0f * (q.x * q.y - q.w * q.z),
             2.0f * (q.x * q.z + q.w * q.y),
             0.0f };
    m[1] = { 2.0f * (q.x * q.y + q.w * q.z),
             1.0f - 2.0f * (q.x * q.x + q.z * q.z),
             2.0f * (q.y * q.z - q.w * q.x),
             0.0f };
    m[2] = { 2.0f * (q.x * q.z - q.w * q.y),
             2.0f * (q.y * q.z + q.w * q.x),
             1.0f - 2.0f * (q.x * q.x + q.y * q.y),
             0.0f };
    m[3] = { -(p.x * m[0].x + p.y * m[1].x + p.z * m[2].x),
             -(p.x * m[0].y + p.y * m[1].y + p.z * m[2].y),
             -(p.x * m[0].z + p.y * m[1].z + p.z * m[2].z),
             1.0f };
}

void mat4::setPerspectiveTrans(const m::perspective &p) {
    const float ar = p.width / p.height;
    const float range = p.nearp - p.farp;
    const float halfFov = tanf(m::toRadian(p.fov / 2.0f));
    m[0] = { 1.0f / (halfFov * ar), 0.0f, 0.0f, 0.0f };
    m[1] = { 0.0f, 1.0f / halfFov, 0.0f, 0.0f };
    m[2] = { 0.0f, 0.0f, (-p.nearp - p.farp) / range, 2.0f * p.farp * p.nearp / range };
    m[3] = { 0.0f, 0.0f, 1.0f, 0.0f };
}

void mat4::getOrient(vec3 *direction, vec3 *up, vec3 *side) const {
    if (side) {
        side->x = m[0][0];
        side->y = m[1][0];
        side->z = m[2][0];
    }
    if (up) {
        up->x = m[0][1];
        up->y = m[1][1];
        up->z = m[2][1];
    }
    if (direction) {
        direction->x = m[0][2];
        direction->y = m[1][2];
        direction->z = m[2][2];
    }
}

// TODO: Cleanup
mat4 mat4::inverse() {
    mat4 r;
    const float *const mat = &m[0][0];
    float *dest = &r.m[0][0];
    const float a00 = mat[0];
    const float a01 = mat[1];
    const float a02 = mat[2];
    const float a03 = mat[3];
    const float a10 = mat[4];
    const float a11 = mat[5];
    const float a12 = mat[6];
    const float a13 = mat[7];
    const float a20 = mat[8];
    const float a21 = mat[9];
    const float a22 = mat[10];
    const float a23 = mat[11];
    const float a30 = mat[12];
    const float a31 = mat[13];
    const float a32 = mat[14];
    const float a33 = mat[15];
    const float b00 = a00 * a11 - a01 * a10;
    const float b01 = a00 * a12 - a02 * a10;
    const float b02 = a00 * a13 - a03 * a10;
    const float b03 = a01 * a12 - a02 * a11;
    const float b04 = a01 * a13 - a03 * a11;
    const float b05 = a02 * a13 - a03 * a12;
    const float b06 = a20 * a31 - a21 * a30;
    const float b07 = a20 * a32 - a22 * a30;
    const float b08 = a20 * a33 - a23 * a30;
    const float b09 = a21 * a32 - a22 * a31;
    const float b10 = a21 * a33 - a23 * a31;
    const float b11 = a22 * a33 - a23 * a32;
    const float d = (b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06);
    const float invDet = 1.0f / d;
    dest[0] = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
    dest[1] = (-a01 * b11 + a02 * b10 - a03 * b09) * invDet;
    dest[2] = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
    dest[3] = (-a21 * b05 + a22 * b04 - a23 * b03) * invDet;
    dest[4] = (-a10 * b11 + a12 * b08 - a13 * b07) * invDet;
    dest[5] = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
    dest[6] = (-a30 * b05 + a32 * b02 - a33 * b01) * invDet;
    dest[7] = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;
    dest[8] = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
    dest[9] = (-a00 * b10 + a01 * b08 - a03 * b06) * invDet;
    dest[10] = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
    dest[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * invDet;
    dest[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * invDet;
    dest[13] = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
    dest[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * invDet;
    dest[15] = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;
    return r;
}

}
