#include "u_misc.h"
#include "u_algorithm.h"

#include "m_vec.h"
#include "m_quat.h"

namespace m {

void vec2::endianSwap() {
    x = u::endianSwap(x);
    y = u::endianSwap(y);
}

vec2 sincos(float x) {
    float sin = 0.0f;
    float cos = 0.0f;
    m::sincos(x, sin, cos);
    return { sin, cos };
}

const vec3 vec3::xAxis(1.0f, 0.0f, 0.0f);
const vec3 vec3::yAxis(0.0f, 1.0f, 0.0f);
const vec3 vec3::zAxis(0.0f, 0.0f, 1.0f);
const vec3 vec3::origin(0.0f, 0.0f, 0.0f);

void vec3::rotate(float angle, const vec3 &axe) {
    const vec2 sc = m::sincos(toRadian(angle * 0.5f));
    const quat rotateQ(axe.x * sc.x, axe.y * sc.x, axe.z * sc.x, sc.y);
    const quat conjugateQ = rotateQ.conjugate();
    const quat W = rotateQ * (*this) * conjugateQ;
    x = W.x;
    y = W.y;
    z = W.z;
}

float vec3::abs() const {
    return m::sqrt(*this * *this);
}

void vec3::endianSwap() {
    x = u::endianSwap(x);
    y = u::endianSwap(y);
    z = u::endianSwap(z);
}

bool vec3::rayCylinderIntersect(const vec3 &start, const vec3 &direction,
        const vec3 &edgeStart, const vec3 &edgeEnd, float radius, float *fraction)
{
    const m::vec3 pa = edgeEnd - edgeStart;
    const m::vec3 s0 = start - edgeStart;
    const float paSquared = pa*pa;
    const float paInvSquared = 1.0f / paSquared;

    // a
    const float pva = direction * pa;
    const float a = (direction*direction) - pva * pva * paInvSquared;
    // b
    const float b = s0 * direction - (s0 * pa) * pva * paInvSquared;
    // c
    const float ps0a = s0 * pa;
    const float c = (s0*s0) - radius * radius - ps0a * ps0a * paInvSquared;

    // test
    const float distance = b*b - a*c;
    if (distance < 0.0f || a == 0.0f)
        return false;
    *fraction = (-b - m::sqrt(distance)) / a;
    const float collide = (start + *fraction * direction - edgeStart) * pa;
    return collide >= 0.0f && collide <= paSquared;
}

bool vec3::raySphereIntersect(const vec3 &start, const vec3 &direction,
    const vec3 &sphere, float radius, float *fraction)
{
    const float a = direction*direction;
    const vec3  s = start - sphere;
    const float b = direction * s;
    const float c1 = s * s;
    const float c4 = radius * radius;
    const float c = c1 - c4;
    const float t = b * b - a * c;
    if (t <= 0.0f)
        return false;
    *fraction = -(b + m::abs(m::sqrt(t))) / a;
    return true;
}

vec3 vec3::rand(float mx, float my, float mz) {
    return { mx * ((float)(u::randu() % 20000) * 0.0001f - 1.0f),
             my * ((float)(u::randu() % 20000) * 0.0001f - 1.0f),
             mz * ((float)(u::randu() % 20000) * 0.0001f - 1.0f) };
}

void vec4::endianSwap() {
    x = u::endianSwap(x);
    y = u::endianSwap(y);
    z = u::endianSwap(z);
    w = u::endianSwap(w);
}

float vec4::abs() const {
    return m::sqrt(dot(*this, *this)); //*this * *this);
}

}
