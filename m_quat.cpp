#include "u_misc.h"

#include "m_quat.h"
#include "m_vec3.h"
#include "m_mat4.h"

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

void quat::rotationAxis(const vec3 &vec, float angle) {
    const float s = sinf(angle / 2.0f);
    const float c = cosf(angle / 2.0f);

    vec3 normal = vec.normalized();
    x = s * normal.x;
    y = s * normal.y;
    z = s * normal.z;
    w = c;
}

void quat::endianSwap() {
    x = u::endianSwap(x);
    y = u::endianSwap(y);
    z = u::endianSwap(z);
    w = u::endianSwap(w);
}

quat operator*(const quat &q, const vec3 &v) {
    const float w = - (q.x * v.x) - (q.y * v.y) - (q.z * v.z);
    const float x =   (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
    const float y =   (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
    const float z =   (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
    return quat(x, y, z, w);
}

quat operator*(const quat &l, const quat &r) {
    const float w = (l.w * r.w) - (l.x * r.x) - (l.y * r.y) - (l.z * r.z);
    const float x = (l.x * r.w) + (l.w * r.x) + (l.y * r.z) - (l.z * r.y);
    const float y = (l.y * r.w) + (l.w * r.y) + (l.z * r.x) - (l.x * r.z);
    const float z = (l.z * r.w) + (l.w * r.z) + (l.x * r.y) - (l.y * r.x);
    return quat(x, y, z, w);
}

void quat::getMatrix(mat4 *mat) const {
    const float n = 1.0f / sqrtf(x*x + y*y + z*z + w*w);
    const float qx = x*n;
    const float qy = y*n;
    const float qz = z*n;
    const float qw = w*n;

    mat->m[0][0] = 1.0f - 2.0f*qy*qy - 2.0f*qz*qz;
    mat->m[0][1] = 2.0f*qx*qy - 2.0f*qz*qw;
    mat->m[0][2] = 2.0f*qx*qz + 2.0f*qy*qw;
    mat->m[0][3] = 0.0f;
    
    mat->m[1][0] = 2.0f*qx*qy + 2.0f*qz*qw;
    mat->m[1][1] = 1.0f - 2.0f*qx*qx - 2.0f*qz*qz;
    mat->m[1][2] = 2.0f*qy*qz - 2.0f*qx*qw;
    mat->m[1][3] = 0.0f;

    mat->m[2][0] = 2.0f*qx*qz - 2.0f*qy*qw;
    mat->m[2][1] = 2.0f*qy*qz + 2.0f*qx*qw;
    mat->m[2][2] = 1.0f - 2.0f*qx*qx - 2.0f*qy*qy;
    mat->m[2][3] = 0.0f;

    mat->m[3][0] = 0.0f;
    mat->m[3][1] = 0.0f;
    mat->m[3][2] = 0.0f;
    mat->m[3][3] = 1.0f;
}

}
