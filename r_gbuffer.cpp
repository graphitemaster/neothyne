#include <string.h>

#include "r_gbuffer.h"

#include "m_mat4.h"

namespace r {

gBuffer::gBuffer()
    : m_fbo(0)
    , m_width(0)
    , m_height(0)
{
    memset(m_textures, 0, sizeof(m_textures));
}

gBuffer::~gBuffer() {
    destroy();
}

void gBuffer::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_textures[0])
        gl::DeleteTextures(kMax, m_textures);
}

void gBuffer::update(const m::perspective &p) {
    size_t width = p.width;
    size_t height = p.height;

    if (m_width != width || m_height != height) {
        GLenum format = gl::has(ARB_texture_rectangle)
            ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
        m_width = width;
        m_height = height;

        // diffuse + specular
        gl::BindTexture(format, m_textures[kColor]);
        gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);

        // normals + specular
        gl::BindTexture(format, m_textures[kNormal]);
        gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);

        // depth
        gl::BindTexture(format, m_textures[kDepth]);
        gl::TexImage2D(format, 0, GL_DEPTH_COMPONENT,
            m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
}

bool gBuffer::init(const m::perspective &p) {
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(kMax, m_textures);

    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // diffuse + specular
    gl::BindTexture(format, m_textures[kColor]);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_textures[kColor], 0);

    // normals + specular
    gl::BindTexture(format, m_textures[kNormal]);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, format, m_textures[kNormal], 0);

    // depth
    gl::BindTexture(format, m_textures[kDepth]);
    gl::TexImage2D(format, 0, GL_DEPTH_COMPONENT,
        m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        format, m_textures[kDepth], 0);

    static GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0, // diffuse + specular intensity
        GL_COLOR_ATTACHMENT1  // normal + specular power
    };

    gl::DrawBuffers(sizeof(drawBuffers)/sizeof(*drawBuffers), drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void gBuffer::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint gBuffer::texture(gBuffer::textureType type) const {
    return m_textures[type];
}

}
