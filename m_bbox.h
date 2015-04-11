#ifndef M_BBOX_HDR
#define M_BBOX_HDR
#include "m_vec.h"

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

}

#endif
