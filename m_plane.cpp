#include "m_plane.h"
#include "m_mat.h"
#include "m_bbox.h"

#include <stdio.h>
namespace m {

void frustum::update(const m::mat4 &wvp) {
    // TODO transpose!
    // left plane
    m_planes[0].n = { wvp.a.w + wvp.a.x, wvp.b.w + wvp.b.x, wvp.c.w + wvp.c.x };
    m_planes[0].d = wvp.d.w + wvp.d.x;
    // right plane
    m_planes[1].n = { wvp.a.w - wvp.a.x, wvp.b.w - wvp.b.x, wvp.c.w - wvp.c.x };
    m_planes[1].d = wvp.d.w - wvp.d.x;
    // top plane
    m_planes[2].n = { wvp.a.w - wvp.a.y, wvp.b.w - wvp.b.y, wvp.c.w - wvp.c.y };
    m_planes[2].d = wvp.d.w - wvp.d.y;
    // bottom plane
    m_planes[3].n = { wvp.a.w + wvp.a.y, wvp.b.w + wvp.b.y, wvp.c.w + wvp.c.y };
    m_planes[3].d = wvp.d.w + wvp.d.y;
    // far plane
    m_planes[4].n = { wvp.a.w - wvp.a.z, wvp.b.w - wvp.b.z, wvp.c.w - wvp.c.z };
    m_planes[4].d = wvp.d.w - wvp.d.z;
    // near plane
    m_planes[5].n = { wvp.a.w + wvp.a.z, wvp.b.w + wvp.b.z, wvp.c.w + wvp.c.z };
    m_planes[5].d = wvp.d.w + wvp.d.z;
    // normalize
    for (size_t i = 0; i < 6; i++) {
        const float magnitude = m_planes[i].n.abs();
        m_planes[i].n.normalize();
        m_planes[i].d *= 1.0f / magnitude;
    }
}

bool frustum::testSphere(const m::vec3 &position, float radius) {
    radius = -radius;
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(position) < radius)
            return false;
    return true;
}

bool frustum::testBox(const m::bbox& box) {
    // TODO: figure out what the fuck is going on here
    return true;
}

bool frustum::testPoint(const m::vec3 &point) {
    for (size_t i = 0; i < 6; i++)
        if (m_planes[i].distance(point) < 0.0f)
            return false;
    return true;
}

}
