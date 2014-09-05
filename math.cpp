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

    ///! sphere
    sphere::sphere()
    {

    }

    sphere::sphere(float radius, size_t rings, size_t sectors) {
        build(radius, rings, sectors);
    }

    void sphere::build(float radius, size_t rings, size_t sectors) {
        const float rr = 1.0f / (float)(rings - 1);
        const float ss = 1.0f / (float)(sectors - 1);

        vertices.resize(rings * sectors * 3);
        auto v = vertices.begin();
        for (size_t r = 0; r < rings; r++) {
            for (size_t s = 0; s < sectors; s++) {
                const float y = sin(-kTau + kPi * r * rr);
                const float x = cos(kTau * s * ss) * sin(kPi * r * rr);
                const float z = sin(kTau * s * ss) * sin(kPi * r * rr);

                *v++ = x * radius;
                *v++ = y * radius;
                *v++ = z * radius;
            }
        }

        indices.resize(rings * sectors * 6);
        auto i = indices.begin();
        for (size_t r = 0; r < rings; r++) {
            for (size_t s = 0; s < sectors; s++) {
                *i++ = r * sectors + s;
                *i++ = (r + 1) * sectors + (s + 1);
                *i++ = r * sectors + (s + 1);
                *i++ = r * sectors + s;
                *i++ = (r + 1) * sectors + s;
                *i++ = (r + 1) * sectors + (s + 1);
            }
        }
    }
}
