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

    // diffuse RGBA
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kDiffuse]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[kDiffuse], 0);

    // normals
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kNormal]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, m_width, m_height, 0, GL_RG, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_textures[kNormal], 0);

    // depth
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kDepth]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, m_textures[kDepth], 0);

    static GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0, // diffuse
        GL_COLOR_ATTACHMENT1  // normal
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
    for (size_t i = 0; i < kMax; i++) {
        gl::ActiveTexture(GL_TEXTURE0 + i);
        gl::BindTexture(GL_TEXTURE_2D, m_textures[i]);
    }
}

void gBuffer::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

}
