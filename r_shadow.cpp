#include "r_shadow.h"

namespace r {

///!shadowMap
shadowMap::shadowMap()
    : m_size(0)
    , m_fbo(0)
    , m_shadowMap(0)
{
}

shadowMap::~shadowMap() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_shadowMap)
        gl::DeleteTextures(1, &m_shadowMap);
}

bool shadowMap::init(size_t size) {
    m_size = size;

    gl::GenFramebuffers(1, &m_fbo);

    gl::GenTextures(1, &m_shadowMap);
    gl::BindTexture(GL_TEXTURE_2D, m_shadowMap);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_size, m_size, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::BindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, m_shadowMap, 0);

    gl::DrawBuffer(GL_NONE);
    gl::ReadBuffer(GL_NONE);

    const GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    return status == GL_FRAMEBUFFER_COMPLETE;
}

void shadowMap::update(size_t size) {
    if (m_size == size)
        return;

    m_size = size;
    gl::BindTexture(GL_TEXTURE_2D, m_shadowMap);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_size, m_size, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

void shadowMap::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint shadowMap::texture() const {
    return m_shadowMap;
}

///! shadowMapMethod
shadowMapMethod::shadowMapMethod()
    : m_WVPLocation(-1)
{
}

bool shadowMapMethod::init() {
    if (!method::init())
        return false;
    if (!addShader(GL_VERTEX_SHADER, "shaders/shadow.vs"))
        return false;
    // No fragment shader needed because gl::DrawBuffer(GL_NONE)
    if (!finalize({ "position" }))
        return false;

    m_WVPLocation = getUniformLocation("gWVP");

    return true;
}

void shadowMapMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

///! shadowMapDebugMethod
shadowMapDebugMethod::shadowMapDebugMethod()
    : m_WVPLocation(-1)
    , m_screenSizeLocation(-1)
    , m_screenFrustumLocation(-1)
    , m_shadowMapTextureUnitLocation(-1)
{
}

bool shadowMapDebugMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/default.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/shadow.fs"))
        return false;
    if (!finalize({ "position" }))
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_shadowMapTextureUnitLocation = getUniformLocation("gShadowMap");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_screenFrustumLocation = getUniformLocation("gScreenFrustum");

    return true;
}

void shadowMapDebugMethod::setShadowMapTextureUnit(int unit) {
    gl::Uniform1i(m_shadowMapTextureUnitLocation, unit);
}

void shadowMapDebugMethod::setWVP(const m::mat4 &wvp) {
    // Will set an identity matrix as this will be a screen space quad
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void shadowMapDebugMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
    gl::Uniform2f(m_screenFrustumLocation, p.nearp, p.farp);
}

}
