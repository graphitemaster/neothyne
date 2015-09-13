#ifndef R_PIPELINE_HDR
#define R_PIPELINE_HDR
#include "m_vec.h"
#include "m_quat.h"
#include "m_mat.h"

namespace r {

struct pipeline {
    pipeline();

    void setScale(const m::vec3 &scale);
    void setWorld(const m::vec3 &worldPosition);
    void setRotate(const m::mat4 &rotate);
    void setPosition(const m::vec3 &position);
    void setRotation(const m::quat &rotation);
    void setPerspective(const m::perspective &p);
    void setTime(float time);
    void setDelta(float delta);

    const m::mat4 &world();
    const m::mat4 &view();
    const m::mat4 &projection();

    // camera accessors.
    const m::vec3 &position() const;
    const m::quat &rotation() const;

    const m::perspective &perspective() const;

    float time() const;
    float delta() const;

private:
    enum {
        kWorld,
        kView,
        kProjection,
        kCount
    };
    m::mat4 m_matrices[kCount];
    m::perspective m_perspective;

    m::vec3 m_scale;
    m::vec3 m_world;
    m::mat4 m_rotate;
    m::vec3 m_position;
    m::quat m_rotation;

    float m_time;
    float m_delta;
};

}

#endif
