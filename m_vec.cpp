#include "u_misc.h"
#include "u_algorithm.h"

#include "m_vec.h"
#include "m_mat.h"
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

const vec3 vec3::kAxis[3] = {
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f }
};

const vec3 &vec3::xAxis = vec3::kAxis[0];
const vec3 &vec3::yAxis = vec3::kAxis[1];
const vec3 &vec3::zAxis = vec3::kAxis[2];

const vec3 vec3::origin(0.0f, 0.0f, 0.0f);

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
    if (distance < 0.0f)
        return false;
    *fraction = (-b - m::sqrt(distance)) / a;
    const float collide = (start + *fraction * direction - edgeStart) * pa;
    return collide >= 0.0f && collide <= paSquared;
}

bool vec3::raySphereIntersect(const vec3 &start, const vec3 &direction,
    const vec3 &sphere, float radius, float *fraction)
{
    // calculate in world space
    const float a = direction*direction;
    const float b = 2.0f*direction*(start - sphere);
    const float c = sphere*sphere+start*start-2.0f*(sphere*start)-radius*radius;
    const float d = b*b - 4.0f*a*c;
    if (d <= 0.0f)
        return false;
    const float e = m::sqrt(d);
    const float u1 = (-b + e)/(2.0f*a);
    const float u2 = -(b + e)/(2.0f*a);
    *fraction = u1  < u2 ? u1 : u2;
    return true;
}

vec3 vec3::rand(float mx, float my, float mz) {
    return { mx * u::randf(),
             my * u::randf(),
             mz * u::randf() };
}

void vec4::endianSwap() {
    x = u::endianSwap(x);
    y = u::endianSwap(y);
    z = u::endianSwap(z);
    w = u::endianSwap(w);
}

#ifdef __SSE2__
float vec4::abs() const {
    __m128 e = _mm_mul_ps(v, v);
    e = _mm_add_ps(e, _mm_pshufd(e, _MM_SHUFFLE(1,0,3,2)));
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_add_ss(e, _mm_pshufd(e, _MM_SHUFFLE(0,1,0,1)))));
}
#else
float vec4::abs() const {
    return m::sqrt(dot(*this, *this));
}
#endif

}
