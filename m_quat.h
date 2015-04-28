#ifndef M_QUAT_HDR
#define M_QUAT_HDR
#include "m_vec.h"

namespace m {

struct mat4;

struct quat : vec4 {
    constexpr quat();
    constexpr quat(const float (&vals)[4]);
    constexpr quat(float x, float y, float z, float w);

    quat(float angle, const vec3 &vec);

    constexpr quat conjugate() const;

    // get all 3 axis of the quaternion
    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;

    // get matrix of this quaternion
    void getMatrix(mat4 *mat) const;

    friend quat operator*(const quat &l, const vec3 &v);
    friend quat operator*(const quat &l, const quat &r);
    friend quat operator*(const quat &l, float k);

    quat normalize() const;

};

inline constexpr quat::quat() = default;

inline constexpr quat::quat(const float (&vals)[4])
    : vec4(vals)
{
}

inline constexpr quat::quat(float x, float y, float z, float w)
    : vec4(x, y, z, w)
{
}

inline quat::quat(float angle, const vec3 &vec) {
    float s;
    float c;
    m::sincos(angle / 2.0f, s, c);

    const vec3 normal = vec.normalized();
    x = s * normal.x;
    y = s * normal.y;
    z = s * normal.z;
    w = c;
}

inline constexpr quat quat::conjugate() const {
    return quat(-x, -y, -z, -w);
}

inline quat quat::normalize() const {
    return *this * (1.0f / abs());
}

}

#endif
