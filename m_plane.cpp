#include "m_plane.h"
#include "m_const.h"
#include "m_mat4.h"
#include "m_quat.h"

namespace m {

void frustum::setup(const m::vec3 &origin, const m::quat &orient, const m::perspectiveProjection &project) {
    m::vec3 direction;
    m::vec3 up;
    m::vec3 side;
    orient.getOrient(&direction, &up, &side);

    direction = -direction;

    const float ratio = project.width / project.height;
    const float angle = m::toDegree(project.fov) * 0.5f;

    const float nh = angle * project.nearp;
    const float nw = nh * ratio;
    const float fh = angle * project.farp;
    const float fw = fh * ratio;

    const m::vec3 farUp = up * fh;
    const m::vec3 farSide = side * fw;
    const m::vec3 nearUp = up * nh;
    const m::vec3 nearSide = side * nw;
    const m::vec3 farPlane = direction * project.farp;
    const m::vec3 nearPlane = direction * project.nearp;

    const m::vec3 ftl = origin + farPlane + farUp - farSide;
    const m::vec3 ftr = origin + farPlane + farUp + farSide;
    const m::vec3 fbl = origin + farPlane - farUp - farSide;
    const m::vec3 fbr = origin + farPlane - farUp + farSide;
    const m::vec3 ntl = origin + nearPlane + nearUp - nearSide;
    //const m::vec3 ntr = origin + nearPlane + nearUp + nearSide;
    const m::vec3 nbl = origin + nearPlane - nearUp - nearSide;
    const m::vec3 nbr = origin + nearPlane - nearUp + nearSide;

    m_planes[kPlaneLeft].setupPlane(fbl, ftl, ntl);
    m_planes[kPlaneRight].setupPlane(ftr, fbr, nbr);
    m_planes[kPlaneUp].setupPlane(ntl, ftl, ftr);
    m_planes[kPlaneDown].setupPlane(fbr, fbl, nbl);
    m_planes[kPlaneNear].setupPlane(origin + nearPlane, direction);
    m_planes[kPlaneFar].setupPlane(fbr, ftr, ftl);
}

}
