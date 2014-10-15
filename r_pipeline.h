#ifndef R_PIPELINE_HDR
#define R_PIPELINE_HDR
#include "m_vec3.h"
#include "m_quat.h"
#include "m_mat4.h"

namespace r {

struct rendererPipeline {
    rendererPipeline();

    void setScale(const m::vec3 &scale);
    void setWorldPosition(const m::vec3 &worldPosition);

    void setRotate(const m::vec3 &rotate);

    void setPosition(const m::vec3 &position);
    void setRotation(const m::quat &rotation);

    void setPerspectiveProjection(const m::perspectiveProjection &projection);

    void setTime(float time);

    const m::mat4 &getWorldTransform();
    const m::mat4 &getWVPTransform();
    const m::mat4 &getVPTransform();
    const m::mat4 &getInverseTransform();

    // camera accessors.
    const m::vec3 &getPosition() const;
    const m::vec3 getTarget() const;
    const m::vec3 getUp() const;
    const m::quat &getRotation() const;

    const m::perspectiveProjection &getPerspectiveProjection() const;

    float getTime() const;

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

    float m_time;
};

}

#endif
