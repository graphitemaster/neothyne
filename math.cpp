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
        const vec3 end = start + direction;
        // a
        const float a = (end.x - start.x) * (end.x - start.x) +
                        (end.y - start.y) * (end.y - start.y) +
                        (end.z - start.z) * (end.z - start.z);
        // b
        const float b = 2.0f * ((end.x - start.x) * (start.x - sphere.x) +
                                (end.y - start.y) * (start.y - sphere.y) +
                                (end.z - start.z) * (start.z - sphere.z));
        // c
        const float c = sphere.x * sphere.x + sphere.y * sphere.y + sphere.z * sphere.z +
                        start.x * start.x + start.y * start.y + start.z * start.z -
                        2.0f * (sphere.x * start.x + sphere.y * start.y + sphere.z * start.z) - radius * radius;

        // test
        const float test = b * b - 4.0f * a * c;
        if (test <= 0.0f)
            return false;
        const float d = sqrtf(test);
        const float u1 = (-b + d) / (2.0f * a);
        const float u2 = -(b + d) / (2.0f * a);
        *fraction = u1 < u2 ? u1 : u2;
        return true;
    }
}
