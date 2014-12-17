#include <string.h>

#include "r_pipeline.h"

namespace r {

pipeline::pipeline()
    : m_scale(1.0f, 1.0f, 1.0f)
    , m_time(0.0f)
{
    m_rotate.setRotateTrans(0.0f, 0.0f, 0.0f);
}

void pipeline::setScale(const m::vec3 &scale) {
    m_scale = scale;
}

void pipeline::setWorld(const m::vec3 &world) {
    m_world = world;
}

void pipeline::setRotate(const m::mat4 &rotate) {
    m_rotate = rotate;
}

void pipeline::setRotation(const m::quat &rotation) {
    m_rotation = rotation;
}

void pipeline::setPosition(const m::vec3 &position) {
    m_position = position;
}

void pipeline::setPerspective(const m::perspective &p) {
    m_perspective = p;
}

void pipeline::setTime(float time) {
    m_time = time;
}

const m::mat4 &pipeline::world() {
    m::mat4 scale;
    m::mat4 translate;
    scale.setScaleTrans(m_scale.x, m_scale.y, m_scale.z);
    translate.setTranslateTrans(m_world.x, m_world.y, m_world.z);
    return m_matrices[kWorld] = translate * m_rotate * scale;
}

const m::mat4 &pipeline::view() {
    m::mat4 translate;
    m::mat4 rotate;
    translate.setTranslateTrans(-m_position.x, -m_position.y, -m_position.z);
    rotate.setCameraTrans(target(), up());
    return m_matrices[kView] = rotate * translate;
}

const m::mat4 &pipeline::projection() {
    m::mat4 perspective;
    perspective.setPerspectiveTrans(m_perspective);
    return m_matrices[kProjection] = perspective;
}

const m::perspective &pipeline::perspective() const {
    return m_perspective;
}

const m::vec3 pipeline::target() const {
    m::vec3 target;
    m_rotation.getOrient(&target, nullptr, nullptr);
    return target;
}

const m::vec3 pipeline::up() const {
    m::vec3 up;
    m_rotation.getOrient(nullptr, &up, nullptr);
    return up;
}

const m::vec3 &pipeline::position() const {
    return m_position;
}

const m::quat &pipeline::rotation() const {
    return m_rotation;
}

float pipeline::time() const {
    return m_time;
}

}
