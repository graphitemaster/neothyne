#include "u_misc.h"

#include "m_quat.h"
#include "m_vec.h"
#include "m_mat.h"

namespace m {

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

quat operator*(const quat &q, const vec3 &v) {
    const float w = - (q.x * v.x) - (q.y * v.y) - (q.z * v.z);
    const float x = (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
    const float y = (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
    const float z = (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
    return { x, y, z, w };
}

quat operator*(const quat &l, const quat &r) {
    const float w = (l.w * r.w) - (l.x * r.x) - (l.y * r.y) - (l.z * r.z);
    const float x = (l.x * r.w) + (l.w * r.x) + (l.y * r.z) - (l.z * r.y);
    const float y = (l.y * r.w) + (l.w * r.y) + (l.z * r.x) - (l.x * r.z);
    const float z = (l.z * r.w) + (l.w * r.z) + (l.x * r.y) - (l.y * r.x);
    return { x, y, z, w };
}

void quat::getMatrix(mat4 *mat) const {
    const float n = 1.0f / sqrtf(x*x + y*y + z*z + w*w);
    const float qx = x*n;
    const float qy = y*n;
    const float qz = z*n;
    const float qw = w*n;
    mat->a = { 1.0f - 2.0f*qy*qy - 2.0f*qz*qz,
               2.0f*qx*qy - 2.0f*qz*qw,
               2.0f*qx*qz + 2.0f*qy*qw,
               0.0f };
    mat->b = { 2.0f*qx*qy + 2.0f*qz*qw,
               1.0f - 2.0f*qx*qx - 2.0f*qz*qz,
               2.0f*qy*qz - 2.0f*qx*qw,
               0.0f };
    mat->c = { 2.0f*qx*qz - 2.0f*qy*qw,
               2.0f*qy*qz + 2.0f*qx*qw,
               1.0f - 2.0f*qx*qx - 2.0f*qy*qy,
               0.0f };
    mat->d = { 0.0f, 0.0f, 0.0f, 1.0f };
}

}
