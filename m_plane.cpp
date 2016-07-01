#include "m_plane.h"
#include "m_mat.h"
#include "m_bbox.h"

#include <stdio.h>
namespace m {

void frustum::update(const m::mat4 &wvp) {
    // left plane
    m_planes[0] = { { wvp.d.x + wvp.a.x, wvp.d.y + wvp.a.y, wvp.d.z + wvp.a.z }, wvp.d.w + wvp.a.w };
    // right plane
    m_planes[1] = { { wvp.d.x - wvp.a.x, wvp.d.y - wvp.a.y, wvp.d.z - wvp.a.z }, wvp.d.w - wvp.a.w };
    // top plane
    m_planes[2] = { { wvp.d.x - wvp.b.x, wvp.d.y - wvp.b.y, wvp.d.z - wvp.b.z }, wvp.d.w - wvp.b.w };
    // bottom plane
    m_planes[3] = { { wvp.d.x + wvp.b.x, wvp.d.y + wvp.b.y, wvp.d.z + wvp.b.z }, wvp.d.w + wvp.b.w };
    // far plane
    m_planes[4] = { { wvp.d.x - wvp.c.x, wvp.d.y - wvp.c.y, wvp.d.z - wvp.c.z }, wvp.d.w - wvp.c.w };
    // near plane
    m_planes[5] = { { wvp.d.x + wvp.c.x, wvp.d.y + wvp.c.y, wvp.d.z + wvp.c.z }, wvp.d.w + wvp.c.w };
}

bool frustum::testSphere(const m::vec3 &position, float radius) {
    radius = -radius;
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(position) < radius)
            return false;
    return true;
}

bool frustum::testBox(const m::bbox& box) {
    const m::vec3 &min = box.min();
    const m::vec3 &max = box.max();
    for (size_t i = 0; i < 6; i++) {
        bool outSide = true;
        for (size_t j = 0; j < 8; j++) {
            m::vec3 v = min;
            if (j & 1) v.x = max.x;
            if (j & 2) v.y = max.y;
            if (j & 4) v.z = max.z;
            if (m_planes[i].distance(v) >= 0.0f) {
                outSide = false;
                break;
            }
        }
        if (outSide)
            return false;
    }
    return true;
}

bool frustum::testPoint(const m::vec3 &point) {
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(point) < 0.0f)
            return false;
    return true;
}

}
