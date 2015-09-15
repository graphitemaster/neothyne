#include "r_shadow.h"

namespace r {

///!shadowMap
shadowMap::shadowMap()
    : m_width(0)
    , m_height(0)
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

bool shadowMap::init(size_t width, size_t height) {
    m_width = width;
    m_height = height;

    gl::GenFramebuffers(1, &m_fbo);

    gl::GenTextures(1, &m_shadowMap);
    gl::BindTexture(GL_TEXTURE_2D, m_shadowMap);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_width, m_height, 0,
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

void shadowMap::update(size_t width, size_t height) {
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    gl::BindTexture(GL_TEXTURE_2D, m_shadowMap);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, m_width, m_height, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}

void shadowMap::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint shadowMap::texture() const {
    return m_shadowMap;
}

float shadowMap::widthScale(size_t size) const {
    return float(size) / float(m_width);
}

float shadowMap::heightScale(size_t size) const {
    return float(size) / float(m_height);
}

///! shadowMapMethod
shadowMapMethod::shadowMapMethod()
    : m_WVP(nullptr)
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

    m_WVP = getUniform("gWVP", uniform::kMat4);

    post();
    return true;
}

void shadowMapMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

}
