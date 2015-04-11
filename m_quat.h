#ifndef M_QUAT_HDR
#define M_QUAT_HDR
#include "m_vec.h"

namespace m {

struct mat4;

struct quat : vec4 {
    constexpr quat() = default;
    constexpr quat(float x, float y, float z, float w);

    quat(float angle, const vec3 &vec);

    constexpr quat conjugate() const;

    // get all 3 axis of the quaternion
    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;

    // get matrix of this quaternion
    void getMatrix(mat4 *mat) const;

    friend quat operator*(const quat &l, const vec3 &v);
    friend quat operator*(const quat &l, const quat &r);

};

inline constexpr quat::quat(float x, float y, float z, float w)
    : vec4(x, y, z, w)
{
}

inline quat::quat(float angle, const vec3 &vec) {
    const float s = sinf(angle / 2.0f);
    const float c = cosf(angle / 2.0f);

    const vec3 normal = vec.normalized();
    x = s * normal.x;
    y = s * normal.y;
    z = s * normal.z;
    w = c;
}

inline constexpr quat quat::conjugate() const {
    return quat(-x, -y, -z, -w);
}

}

#endif
