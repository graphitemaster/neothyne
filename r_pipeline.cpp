#include "r_pipeline.h"

rendererPipeline::rendererPipeline(void) :
    m_scale(1.0f, 1.0f, 1.0f),
    m_worldPosition(0.0f, 0.0f, 0.0f),
    m_rotate(0.0f, 0.0f, 0.0f)
{
}

void rendererPipeline::setScale(const m::vec3 &scale) {
    m_scale = scale;
}

void rendererPipeline::setWorldPosition(const m::vec3 &worldPosition) {
    m_worldPosition = worldPosition;
}

void rendererPipeline::setRotate(const m::vec3 &rotate) {
    m_rotate = rotate;
}

void rendererPipeline::setRotation(const m::quat &rotation) {
    m_rotation = rotation;
}

void rendererPipeline::setPosition(const m::vec3 &position) {
    m_position = position;
}

void rendererPipeline::setPerspectiveProjection(const m::perspectiveProjection &projection) {
    m_perspectiveProjection = projection;
}

const m::mat4 &rendererPipeline::getWorldTransform(void) {
    m::mat4 scale, rotate, translate;
    scale.setScaleTrans(m_scale.x, m_scale.y, m_scale.z);
    rotate.setRotateTrans(m_rotate.x, m_rotate.y, m_rotate.z);
    translate.setTranslateTrans(m_worldPosition.x, m_worldPosition.y, m_worldPosition.z);

    m_worldTransform = translate * rotate * scale;
    return m_worldTransform;
}

const m::mat4 &rendererPipeline::getVPTransform(void) {
    m::mat4 translate, rotate, perspective;
    translate.setTranslateTrans(-m_position.x, -m_position.y, -m_position.z);
    rotate.setCameraTrans(getTarget(), getUp());
    perspective.setPersProjTrans(getPerspectiveProjection());
    m_VPTransform = perspective * rotate * translate;
    return m_VPTransform;
}

const m::mat4 &rendererPipeline::getWVPTransform(void) {
    getWorldTransform();
    getVPTransform();

    m_WVPTransform = m_VPTransform * m_worldTransform;
    return m_WVPTransform;
}

const m::mat4 &rendererPipeline::getInverseTransform(void) {
    getVPTransform();
    m_inverseTransform = m_VPTransform.inverse();
    return m_inverseTransform;
}

const m::perspectiveProjection &rendererPipeline::getPerspectiveProjection(void) const {
    return m_perspectiveProjection;
}

const m::vec3 rendererPipeline::getTarget(void) const {
    m::vec3 target;
    m_rotation.getOrient(&target, nullptr, nullptr);
    return target;
}

const m::vec3 rendererPipeline::getUp(void) const {
    m::vec3 up;
    m_rotation.getOrient(nullptr, &up, nullptr);
    return up;
}

const m::vec3 &rendererPipeline::getPosition(void) const {
    return m_position;
}

const m::quat &rendererPipeline::getRotation(void) const {
    return m_rotation;
}