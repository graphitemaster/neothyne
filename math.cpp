#include "math.h"

namespace m {
    ///! vec3
    const vec3 vec3::xAxis(1.0f, 0.0f, 0.0f);
    const vec3 vec3::yAxis(0.0f, 1.0f, 0.0f);
    const vec3 vec3::zAxis(0.0f, 0.0f, 1.0f);

    void vec3::rotate(float angle, const vec3 &axe) {
        const float s = sinf(m::toRadian(angle / 2.0f));
        const float c = cosf(m::toRadian(angle / 2.0f));
        const float rx = axe.x * s;
        const float ry = axe.y * s;
        const float rz = axe.z * s;
        const float rw = c;
        quat rotateQ(rx, ry, rz, rw);
        quat conjugateQ = rotateQ.conjugate();
        quat W = rotateQ * (*this) * conjugateQ;
        x = W.x, y = W.y, z = W.z;
    }

    bool vec3::rayCylinderIntersect(const vec3 &start, const vec3 &direction,
            const vec3 &edgeStart, const vec3 &edgeEnd, float radius, float *fraction)
    {
        const m::vec3 pa = edgeEnd - edgeStart;
        const m::vec3 s0 = start - edgeStart;
        const float paSquared = pa.absSquared();
        const float paInvSquared = 1.0f / paSquared;

        // a
        const float pva = direction * pa;
        const float a = direction.absSquared() - pva * pva * paInvSquared;
        // b
        const float b = s0 * direction - (s0 * pa) * pva * paInvSquared;
        // c
        const float ps0a = s0 * pa;
        const float c = s0.absSquared() - radius * radius - ps0a * ps0a * paInvSquared;

        // test
        const float distance = b*b - a*c;
        if (distance < 0.0f)
            return false;
        *fraction = (-b - sqrtf(distance)) / a;
        const float collide = (start + *fraction * direction - edgeStart) * pa;
        return collide >= 0.0f && collide <= paSquared;
    }

    bool vec3::raySphereIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &sphere, float radius, float *fraction)
    {
        const float a = direction.absSquared();
        const vec3  s = start - sphere;
        const float b = direction * s;
        const float c1 = s.absSquared();
        const float c4 = radius * radius;
        const float c = c1 - c4;
        const float t = b * b - a * c;
        if (t <= 0.0f)
            return false;
        *fraction = -(b + fabs(sqrtf(t))) / a;
        return true;
    }

    ///! mat4
    void mat4::loadIdentity(void) {
        m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
        m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
        m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
        m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
    }

    mat4 mat4::operator*(const mat4 &t) const {
        mat4 r;
        r.m[0][0] = m[0][0] * t.m[0][0] + m[0][1] * t.m[1][0] + m[0][2] * t.m[2][0] + m[0][3] * t.m[3][0];
        r.m[0][1] = m[0][0] * t.m[0][1] + m[0][1] * t.m[1][1] + m[0][2] * t.m[2][1] + m[0][3] * t.m[3][1];
        r.m[0][2] = m[0][0] * t.m[0][2] + m[0][1] * t.m[1][2] + m[0][2] * t.m[2][2] + m[0][3] * t.m[3][2];
        r.m[0][3] = m[0][0] * t.m[0][3] + m[0][1] * t.m[1][3] + m[0][2] * t.m[2][3] + m[0][3] * t.m[3][3];
        r.m[1][0] = m[1][0] * t.m[0][0] + m[1][1] * t.m[1][0] + m[1][2] * t.m[2][0] + m[1][3] * t.m[3][0];
        r.m[1][1] = m[1][0] * t.m[0][1] + m[1][1] * t.m[1][1] + m[1][2] * t.m[2][1] + m[1][3] * t.m[3][1];
        r.m[1][2] = m[1][0] * t.m[0][2] + m[1][1] * t.m[1][2] + m[1][2] * t.m[2][2] + m[1][3] * t.m[3][2];
        r.m[1][3] = m[1][0] * t.m[0][3] + m[1][1] * t.m[1][3] + m[1][2] * t.m[2][3] + m[1][3] * t.m[3][3];
        r.m[2][0] = m[2][0] * t.m[0][0] + m[2][1] * t.m[1][0] + m[2][2] * t.m[2][0] + m[2][3] * t.m[3][0];
        r.m[2][1] = m[2][0] * t.m[0][1] + m[2][1] * t.m[1][1] + m[2][2] * t.m[2][1] + m[2][3] * t.m[3][1];
        r.m[2][2] = m[2][0] * t.m[0][2] + m[2][1] * t.m[1][2] + m[2][2] * t.m[2][2] + m[2][3] * t.m[3][2];
        r.m[2][3] = m[2][0] * t.m[0][3] + m[2][1] * t.m[1][3] + m[2][2] * t.m[2][3] + m[2][3] * t.m[3][3];
        r.m[3][0] = m[3][0] * t.m[0][0] + m[3][1] * t.m[1][0] + m[3][2] * t.m[2][0] + m[3][3] * t.m[3][0];
        r.m[3][1] = m[3][0] * t.m[0][1] + m[3][1] * t.m[1][1] + m[3][2] * t.m[2][1] + m[3][3] * t.m[3][1];
        r.m[3][2] = m[3][0] * t.m[0][2] + m[3][1] * t.m[1][2] + m[3][2] * t.m[2][2] + m[3][3] * t.m[3][2];
        r.m[3][3] = m[3][0] * t.m[0][3] + m[3][1] * t.m[1][3] + m[3][2] * t.m[2][3] + m[3][3] * t.m[3][3];
        return r;
    }

    void mat4::setScaleTrans(float scaleX, float scaleY, float scaleZ) {
        m[0][0] = scaleX; m[0][1] = 0.0f;   m[0][2] = 0.0f;   m[0][3] = 0.0f;
        m[1][0] = 0.0f;   m[1][1] = scaleY; m[1][2] = 0.0f;   m[1][3] = 0.0f;
        m[2][0] = 0.0f;   m[2][1] = 0.0f;   m[2][2] = scaleZ; m[2][3] = 0.0f;
        m[3][0] = 0.0f;   m[3][1] = 0.0f;   m[3][2] = 0.0f;   m[3][3] = 1.0f;
    }

    void mat4::setRotateTrans(float rotateX, float rotateY, float rotateZ) {
        mat4 rx, ry, rz;

        const float x = toRadian(rotateX);
        const float y = toRadian(rotateY);
        const float z = toRadian(rotateZ);

        rx.m[0][0] = 1.0f;    rx.m[0][1] = 0.0f;     rx.m[0][2] = 0.0f;     rx.m[0][3] = 0.0f;
        rx.m[1][0] = 0.0f;    rx.m[1][1] = cosf(x);  rx.m[1][2] = -sinf(x); rx.m[1][3] = 0.0f;
        rx.m[2][0] = 0.0f;    rx.m[2][1] = sinf(x);  rx.m[2][2] = cosf(x);  rx.m[2][3] = 0.0f;
        rx.m[3][0] = 0.0f;    rx.m[3][1] = 0.0f;     rx.m[3][2] = 0.0f;     rx.m[3][3] = 1.0f;
        ry.m[0][0] = cosf(y); ry.m[0][1] = 0.0f;     ry.m[0][2] = -sinf(y); ry.m[0][3] = 0.0f;
        ry.m[1][0] = 0.0f;    ry.m[1][1] = 1.0f;     ry.m[1][2] = 0.0f;     ry.m[1][3] = 0.0f;
        ry.m[2][0] = sinf(y); ry.m[2][1] = 0.0f;     ry.m[2][2] = cosf(y);  ry.m[2][3] = 0.0f;
        ry.m[3][0] = 0.0f;    ry.m[3][1] = 0.0f;     ry.m[3][2] = 0.0f;     ry.m[3][3] = 1.0f;
        rz.m[0][0] = cosf(z); rz.m[0][1] = -sinf(z); rz.m[0][2] = 0.0f;     rz.m[0][3] = 0.0f;
        rz.m[1][0] = sinf(z); rz.m[1][1] = cosf(z);  rz.m[1][2] = 0.0f;     rz.m[1][3] = 0.0f;
        rz.m[2][0] = 0.0f;    rz.m[2][1] = 0.0f;     rz.m[2][2] = 1.0f;     rz.m[2][3] = 0.0f;
        rz.m[3][0] = 0.0f;    rz.m[3][1] = 0.0f;     rz.m[3][2] = 0.0f;     rz.m[3][3] = 1.0f;

        *this = rz * ry * rx;
    }

    void mat4::setTranslateTrans(float x, float y, float z) {
        m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = x;
        m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = y;
        m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = z;
        m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
    }

    void mat4::setCameraTrans(const vec3 &target, const vec3 &up) {
        vec3 N = target.normalized();
        vec3 U = up.normalized().cross(N);
        vec3 V = N.cross(U);
        m[0][0] = U.x;  m[0][1] = U.y;  m[0][2] = U.z;  m[0][3] = 0.0f;
        m[1][0] = V.x;  m[1][1] = V.y;  m[1][2] = V.z;  m[1][3] = 0.0f;
        m[2][0] = N.x;  m[2][1] = N.y;  m[2][2] = N.z;  m[2][3] = 0.0f;
        m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
    }

    void mat4::setCameraTrans(const vec3 &position, const quat &q) {
        m[0][0] = 1.0f - 2.0f * (q.y * q.y + q.z*q.z);
        m[1][0] = 2.0f * (q.x * q.y + q.w * q.z);
        m[2][0] = 2.0f * (q.x * q.z - q.w * q.y);
        m[0][1] = 2.0f * (q.x * q.y - q.w * q.z);
        m[1][1] = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
        m[2][1] = 2.0f * (q.y * q.z + q.w * q.x);
        m[0][2] = 2.0f * (q.x * q.z + q.w * q.y);
        m[1][2] = 2.0f * (q.y * q.z - q.w * q.x);
        m[2][2] = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        m[3][0] = -(position.x * m[0][0] + position.y * m[1][0] + position.z * m[2][0]);
        m[3][1] = -(position.x * m[0][1] + position.y * m[1][1] + position.z * m[2][1]);
        m[3][2] = -(position.x * m[0][2] + position.y * m[1][2] + position.z * m[2][2]);
        m[0][3] = 0.0f;
        m[1][3] = 0.0f;
        m[2][3] = 0.0f;
        m[3][3] = 1.0f;
    }

    void mat4::setPersProjTrans(const perspectiveProjection &projection) {
        const float ar = projection.width / projection.height;
        const float range = projection.nearp - projection.farp;
        const float halfFov = tanf(m::toRadian(projection.fov / 2.0f));

        m[0][0] = 1.0f/(halfFov * ar);
        m[0][1] = 0.0f;
        m[0][2] = 0.0f;
        m[0][3] = 0.0f;
        m[1][0] = 0.0f;
        m[1][1] = 1.0f/halfFov;
        m[1][2] = 0.0f;
        m[1][3] = 0.0f;
        m[2][0] = 0.0f;
        m[2][1] = 0.0f;
        m[2][2] = (-projection.nearp -projection.farp) / range;
        m[2][3] = 2.0f * projection.farp * projection.nearp / range;
        m[3][0] = 0.0f;
        m[3][1] = 0.0f;
        m[3][2] = 1.0f;
        m[3][3] = 0.0f;
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

    mat4 mat4::inverse(void) {
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

    ///! quat
    void quat::getOrient(vec3 *direction, vec3 *up, vec3 *side) const {
        if (side) {
            side->x = 1.0f - 2.0f * (y * y + z * z);
            side->y = 2.0f * (x * y + w * z);
            side->z = 2.0f * (x * z - w * y);
        }
        if (up) {
            up->x = 2.0f * (x * y - w * z);
            up->y = 1.0f - 2.0f * (x * x + z * z);
            up->z = 2.0f * (y * z + w * x);
        }
        if (direction) {
            direction->x = 2.0f * (x * z + w * y);
            direction->y = 2.0f * (y * z - w * x);
            direction->z = 1.0f - 2.0f * (x * x + y * y);
        }
    }

    void quat::rotationAxis(vec3 vec, float angle) {
        const float s = sinf(angle / 2.0f);
        const float c = cosf(angle / 2.0f);

        vec.normalize();
        x = s * vec.x;
        y = s * vec.y;
        z = s * vec.z;
        w = c;
    }
}
