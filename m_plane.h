#ifndef M_PLANE_HDR
#define M_PLANE_HDR
#include "m_vec.h"

namespace m {

struct quat;
struct perspective;

struct plane {
    enum point {
        kBack,
        kOn,
        kFront
    };

    plane();
    plane(const m::vec3 &p1, const m::vec3 &p2, const m::vec3 &p3);
    plane(const m::vec3 &pp, const m::vec3 &nn);

    bool intersect(float &f, const m::vec3 &p, const m::vec3 &v) const;

    float distance(const m::vec3 &p) const;
    point classify(const m::vec3 &p, float epsilon = kEpsilon) const;

    vec3 n;
    float d;
};

inline plane::plane()
    : d(0.0f)
{
}

inline plane::plane(const m::vec3 &p1, const m::vec3 &p2, const m::vec3 &p3)
    : n(((p2 - p1) ^ (p3 - p1)).normalized())
    , d(-n * p1)
{
}

inline plane::plane(const m::vec3 &point, const m::vec3 &normal)
    : n(normal.normalized())
    , d(-n * point)
{
}

inline bool plane::intersect(float &f, const m::vec3 &p, const m::vec3 &v) const {
    const float q = n * v;
    // Plane and line are parallel?
    if (m::abs(q) < kEpsilon)
        return false;
    f = -(n * p + d) / q;
    return true;
}

inline float plane::distance(const m::vec3 &p) const {
    return p * n + d;
}

inline plane::point plane::classify(const m::vec3 &p, float epsilon) const {
    const float dist = distance(p);
    return dist > epsilon ? kFront : dist < -epsilon ? kBack : kOn;
}

}

#endif
