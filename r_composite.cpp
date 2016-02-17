#include "r_composite.h"

namespace r {

composite::composite()
    : m_fbo(0)
    , m_texture(0)
    , m_width(0)
    , m_height(0)
{
}

composite::~composite() {
    destroy();
}

void composite::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
}

void composite::update(const m::perspective &p) {
    const size_t width = p.width;
    const size_t height = p.height;

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;
    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
        GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool composite::init(const m::perspective &p, GLuint depth) {
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(1, &m_texture);

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // output composite
    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT,
        nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format,
        m_texture, 0);

    gl::BindTexture(format, depth);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        format, depth, 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    gl::DrawBuffers(1, drawBuffers);

    const GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void composite::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint composite::texture() const {
    return m_texture;
}

}
