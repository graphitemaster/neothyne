#include <string.h>

#include "r_grader.h"
#include "m_mat.h"

namespace r {

grader::grader()
    : m_fbo(0)
    , m_width(0)
    , m_height(0)
{
    memset(m_textures, 0, sizeof m_textures);
}

grader::~grader() {
    destroy();
}

void grader::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_textures[0])
        gl::DeleteTextures(2, m_textures);
}

void grader::update(const m::perspective &p, const unsigned char *const colorGradingData) {
    const size_t width = p.width;
    const size_t height = p.height;

    // Need to update color grading data if any is passed at all
    if (colorGradingData) {
        gl::BindTexture(GL_TEXTURE_3D, m_textures[kColorGrading]);
        gl::TexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 16, 16, 16, GL_RGB,
            GL_UNSIGNED_BYTE, colorGradingData);
    }

    if (m_width == width && m_height == height)
        return;

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    m_width = width;
    m_height = height;

    gl::BindTexture(format, m_textures[kOutput]);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
}

bool grader::init(const m::perspective &p, const unsigned char *const colorGradingData) {
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(2, m_textures);

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::BindTexture(format, m_textures[kOutput]);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_textures[kOutput], 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

    gl::DrawBuffers(sizeof drawBuffers / sizeof *drawBuffers, drawBuffers);

    const GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    // Color grading texture
    gl::BindTexture(GL_TEXTURE_3D, m_textures[kColorGrading]);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl::TexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 16, 16, 16, 0, GL_RGB,
        GL_UNSIGNED_BYTE, colorGradingData);

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void grader::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint grader::texture(type what) const {
    return m_textures[what];
}

}
