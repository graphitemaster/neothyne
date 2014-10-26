#include <string.h>

#include "r_gbuffer.h"

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

void gBuffer::update(const m::perspectiveProjection &project) {
    size_t width = project.width;
    size_t height = project.height;

    if (m_width != width || m_height != height) {
        destroy();
        init(project);
    }
}

bool gBuffer::init(const m::perspectiveProjection &project) {
    m_width = project.width;
    m_height = project.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(kMax, m_textures);

    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // diffuse
    gl::BindTexture(format, m_textures[kDiffuse]);
    gl::TexImage2D(format, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_textures[kDiffuse], 0);

    // normals
    gl::BindTexture(format, m_textures[kNormal]);
    gl::TexImage2D(format, 0, GL_RG16F, m_width, m_height, 0, GL_RG, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, format, m_textures[kNormal], 0);

    // specularity
    gl::BindTexture(format, m_textures[kSpec]);
    gl::TexImage2D(format, 0, GL_RG16F, m_width, m_height, 0, GL_RG, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, format, m_textures[kSpec], 0);

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
        GL_COLOR_ATTACHMENT0, // diffuse
        GL_COLOR_ATTACHMENT1, // normal
        GL_COLOR_ATTACHMENT2  // speclarity
    };

    gl::DrawBuffers(sizeof(drawBuffers)/sizeof(*drawBuffers), drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void gBuffer::bindReading() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    for (size_t i = 0; i < kMax; i++) {
        gl::ActiveTexture(GL_TEXTURE0 + i);
        gl::BindTexture(format, m_textures[i]);
    }
}

void gBuffer::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

}
