#ifndef M_BBOX_HDR
#define M_BBOX_HDR
#include <float.h>

#include "m_vec.h"
#include "m_mat.h"

namespace m {

struct bbox {
    bbox();
    bbox(const vec3 &min, const vec3 &max);
    bbox(const vec3 &point);

    void expand(const vec3 &point);
    void expand(const bbox &box);

    float area() const;
    vec3 center() const;
    vec3 size() const;
    const vec3 &min() const;
    const vec3 &max() const;

    bbox &transform(const m::mat4 &mat);

private:
    vec3 m_min;
    vec3 m_max;
    vec3 m_extent;
};

inline bbox::bbox() = default;

inline bbox::bbox(const vec3 &min, const vec3 &max)
    : m_min(min)
    , m_max(max)
    , m_extent(max - min)
{
}

inline bbox::bbox(const vec3 &point)
    : m_min(point)
    , m_max(point)
{
}

inline void bbox::expand(const vec3 &point) {
    m_min = vec3::min(m_min, point);
    m_max = vec3::max(m_max, point);
    m_extent = m_max - m_min;
}

inline void bbox::expand(const bbox &box) {
    m_min = vec3::min(m_min, box.m_min);
    m_max = vec3::max(m_max, box.m_max);
    m_extent = m_max - m_min;
}

inline float bbox::area() const {
    return 2.0f * (m_extent.x*m_extent.z + m_extent.x*m_extent.y + m_extent.y*m_extent.z);
}

inline vec3 bbox::center() const {
    return (m_min + m_max) / 2.0f;
}

inline vec3 bbox::size() const {
    return m_extent;
}

inline const vec3 &bbox::min() const {
    return m_min;
}

inline const vec3 &bbox::max() const {
    return m_max;
}

inline bbox &bbox::transform(const m::mat4 &mat) {
    const m::vec3 x = { mat.a.x, mat.b.x, mat.c.x };
    const m::vec3 y = { mat.a.y, mat.b.y, mat.c.y };
    const m::vec3 z = { mat.a.z, mat.b.z, mat.c.z };
    const m::vec3 w = { mat.a.w, mat.b.w, mat.c.w };
    const m::vec3 minx = m::vec3::min(x * m_min.x, x * m_max.x);
    const m::vec3 maxx = m::vec3::max(x * m_min.x, x * m_max.x);
    const m::vec3 miny = m::vec3::min(y * m_min.y, y * m_max.y);
    const m::vec3 maxy = m::vec3::max(y * m_min.y, y * m_max.y);
    const m::vec3 minz = m::vec3::min(z * m_min.z, z * m_max.z);
    const m::vec3 maxz = m::vec3::max(z * m_min.z, z * m_max.z);
    m_min = minx + miny + minz + w;
    m_max = maxx + maxy + maxz + w;
    m_extent = m_max - m_min;
    return *this;
}

}

#endif
