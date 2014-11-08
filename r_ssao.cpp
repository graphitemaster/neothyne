#include "r_ssao.h"

namespace r {

ssao::ssao()
    : m_fbo(0)
    , m_texture(0)
    , m_width(0)
    , m_height(0)
{
}

ssao::~ssao() {
    destroy();
}

bool ssao::init(const m::perspectiveProjection &project) {
    m_width = project.width;
    m_height = project.height;

    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::GenTextures(1, &m_texture);

    // 16-bit RED
    gl::BindTexture(format, m_texture);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameterf(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexImage2D(format, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_texture, 0);

    static GLuint drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

    gl::DrawBuffers(1, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void ssao::update(const m::perspectiveProjection &project) {
    size_t width = project.width;
    size_t height = project.height;

    if (m_width != width || m_height != height) {
        destroy();
        init(project);
    }
}

void ssao::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
}

void ssao::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint ssao::texture() const {
    return m_texture;
}

}
