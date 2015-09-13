#include <string.h>

#include "r_pipeline.h"

namespace r {

pipeline::pipeline()
    : m_scale(1.0f, 1.0f, 1.0f)
    , m_time(0.0f)
    , m_delta(0.0f)
{
    m_rotate = m::mat4::rotate(m::vec3::origin);
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

void pipeline::setDelta(float delta) {
    m_delta = delta;
}

const m::mat4 &pipeline::world() {
    return m_matrices[kWorld] = m::mat4::translate(m_world) * m_rotate * m::mat4::scale(m_scale);
}

const m::mat4 &pipeline::view() {
    return m_matrices[kView] = m::mat4::lookat(target(), up()) * m::mat4::translate(-m_position);
}

const m::mat4 &pipeline::projection() {
    return m_matrices[kProjection] = m::mat4::project(m_perspective);
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

float pipeline::delta() const {
    return m_delta;
}

}
