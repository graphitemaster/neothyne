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

    plane() = default;
    plane(const m::vec3 &p1, const m::vec3 &p2, const m::vec3 &p3);
    plane(const m::vec3 &pp, const m::vec3 &nn);

    bool intersect(float &f, const m::vec3 &p, const m::vec3 &v) const;

    float distance(const m::vec3 &p) const;
    point classify(const m::vec3 &p, float epsilon = kEpsilon) const;

    vec3 n;
    float d;
};

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

struct frustum {
    void setup(const m::vec3 &origin, const m::quat &orient, const m::perspective &project);
    bool testSphere(const m::vec3 &point, float radius) const;
    bool testPoint(const m::vec3 &point) const;
private:
    enum {
        kPlaneNear,
        kPlaneLeft,
        kPlaneRight,
        kPlaneUp,
        kPlaneDown,
        kPlaneFar,
        kPlanes
    };
    plane m_planes[kPlanes];
};

inline bool frustum::testSphere(const m::vec3 &point, float radius) const {
    radius = -radius;
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(point) < radius)
            return false;
    return true;
}

inline bool frustum::testPoint(const m::vec3 &point) const {
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(point) <= 0.0f)
            return false;
    return true;
}

}

#endif
