#ifndef R_PIPELINE_HDR
#define R_PIPELINE_HDR
#include "math.h"

namespace r {

struct rendererPipeline {
    rendererPipeline(void);

    void setScale(const m::vec3 &scale);
    void setWorldPosition(const m::vec3 &worldPosition);

    void setRotate(const m::vec3 &rotate);

    void setPosition(const m::vec3 &position);
    void setRotation(const m::quat &rotation);

    void setPerspectiveProjection(const m::perspectiveProjection &projection);

    const m::mat4 &getWorldTransform(void);
    const m::mat4 &getWVPTransform(void);
    const m::mat4 &getVPTransform(void);
    const m::mat4 &getInverseTransform(void);

    // camera accessors.
    const m::vec3 &getPosition(void) const;
    const m::vec3 getTarget(void) const;
    const m::vec3 getUp(void) const;
    const m::quat &getRotation(void) const;

    const m::perspectiveProjection &getPerspectiveProjection(void) const;

private:
    m::perspectiveProjection m_perspectiveProjection;

    m::vec3 m_scale;
    m::vec3 m_worldPosition;
    m::vec3 m_rotate;
    m::vec3 m_position;

    m::mat4 m_worldTransform;
    m::mat4 m_WVPTransform;
    m::mat4 m_VPTransform;
    m::mat4 m_inverseTransform;

    m::quat m_rotation;
};

}

#endif
