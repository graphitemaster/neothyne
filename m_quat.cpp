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

#ifdef __SSE2__
#ifdef __SSE3__
#include <immintrin.h>
#else
static inline __m128 _mm_addsub_ps(__m128 a, __m128 b) {
    const static __m128 n = _mm_set_ps(1,-1,1,-1);
    b = _mm_mul_ps(b, n); // 5/4, 1
    a = _mm_add_ps(a, b); // 5/4, 1
    return a;
}
#endif

quat operator*(const quat &l, const quat &r) {
    // ~18 cycles if SSE3 is supported (addsubps)
    // ~21 cycles for SSE2
    const __m128 t0 = _mm_shufd(l.v, _MM_SHUFFLE(3,3,3,3)); // 1, 0.5
    const __m128 t1 = _mm_shufd(r.v, _MM_SHUFFLE(2,3,0,1)); // 1, 0.5
    const __m128 t3 = _mm_shufd(l.v, _MM_SHUFFLE(0,0,0,0)); // 1, 0.5
    const __m128 t4 = _mm_shufd(r.v, _MM_SHUFFLE(1,0,3,2)); // 1, 0.5
    const __m128 t5 = _mm_shufd(l.v, _MM_SHUFFLE(1,1,1,1)); // 1, 0.5
    const __m128 t6 = _mm_shufd(r.v, _MM_SHUFFLE(2,0,3,1)); // 1, 0.5
    // [d,d,d,d]*[z,w,x,y] = [dz,dw,dx,dy]
    const __m128 m0 = _mm_mul_ps(t0, t1); // 5/4, 1
    // [a,a,a,a]*[y,x,w,z] = [ay,ax,aw,az]
    const __m128 m1 = _mm_mul_ps(t3, t4); // 5/4, 1
    // [b,b,b,b]*[z,x,w,y] = [bz,bx,bw,by]
    const __m128 m2 = _mm_mul_ps(t5, t6); // 5/4, 1
    // [c,c,c,c]*[w,z,x,y] = [cw,cz,cx,cy]
    const __m128 t7 = _mm_shufd(l.v, _MM_SHUFFLE(2,2,2,2)); // 1, 0.5
    const __m128 t8 = _mm_shufd(r.v, _MM_SHUFFLE(3,2,0,1)); // 1, 0.5
    const __m128 m3 = _mm_mul_ps(t7, t8); // 5/4, 1
    // [dz,dw,dx,dy]+-[ay,ax,sw,az] = [dz+ay,dw-ax,dx+aw,dy-az]
    __m128 e = _mm_addsub_ps(m0, m1); // 3, 1
    // [dx+aw,dz+ay,dy-az,dw-ax]
    e = _mm_shufd(e, _MM_SHUFFLE(1, 3, 0, 2)); // 1, 0.5
    // [dx+aw,dz+ay,dy-az,dw-ax]+-[bz,bx,bw,by] = [dx+aw+bz,dz+ay-bx,dy-az+bw,dw-ax-by]
    e = _mm_addsub_ps(e, m2); // 3, 1
    // [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz]
    e = _mm_shufd(e, _MM_SHUFFLE(2, 0, 1, 3)); // 1, 0.5
    // [dz+ay-bx,dw-ax-by,dy-az+bw,dx+aw+bz]+-[cw,cz,cx,cy] = [dz+ay-bx+cw,dw-ax-by-cz,dy-az+bw+cx,dx+aw+bz-cy]
    e = _mm_addsub_ps(e, m3); // 3, 1
    // [dw-ax-by-cz,dz+ay-bx+cw,dy-az+bw+cx,dx+aw+bz-cy]
    return _mm_shufd(e, _MM_SHUFFLE(2, 3, 1, 0)); // 1, 0.5
}
#else
quat operator*(const quat &l, const quat &r) {
    const float w = (l.w * r.w) - (l.x * r.x) - (l.y * r.y) - (l.z * r.z);
    const float x = (l.x * r.w) + (l.w * r.x) + (l.y * r.z) - (l.z * r.y);
    const float y = (l.y * r.w) + (l.w * r.y) + (l.z * r.x) - (l.x * r.z);
    const float z = (l.z * r.w) + (l.w * r.z) + (l.x * r.y) - (l.y * r.x);
    return { x, y, z, w };
}
#endif

quat operator*(const quat &q, const vec3 &v) {
    const float w = - (q.x * v.x) - (q.y * v.y) - (q.z * v.z);
    const float x = (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
    const float y = (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
    const float z = (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
    return { x, y, z, w };
}

quat operator*(const quat &l, float k) {
    return { l.x*k, l.y*k, l.z*k, l.w*k };
}

void quat::getMatrix(mat4 *mat) const {
    const float n = 1.0f / m::sqrt(x*x + y*y + z*z + w*w);
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
