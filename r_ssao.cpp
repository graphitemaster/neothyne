#include <math.h>

#include "r_ssao.h"

#include "m_const.h"

#include "u_vector.h"
#include "u_misc.h"

namespace r {

bool ssaoMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    method::define("kKernelSize", kKernelSize);
    method::define("kKernelFactor", sinf(m::kPi / float(kKernelSize)));
    method::define("kKernelOffset", 1.0f / kKernelSize);

    if (!addShader(GL_VERTEX_SHADER, "shaders/ssao.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/ssao.fs"))
        return false;
    if (!finalize({ { 0, "position" } }))
        return false;

    m_occluderBiasLocation = getUniformLocation("gOccluderBias");
    m_samplingRadiusLocation = getUniformLocation("gSamplingRadius");
    m_attenuationLocation = getUniformLocation("gAttenuation");
    m_inverseLocation = getUniformLocation("gInverse");
    m_WVPLocation = getUniformLocation("gWVP");
    m_screenFrustumLocation = getUniformLocation("gScreenFrustum");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_normalTextureLocation = getUniformLocation("gNormalMap");
    m_depthTextureLocation = getUniformLocation("gDepthMap");
    m_randomTextureLocation = getUniformLocation("gRandomMap");

    for (size_t i = 0; i < kKernelSize; i++)
        m_kernelLocation[i] = getUniformLocation(u::format("gKernel[%zu]", i));

    // Setup the kernel
    // Note: This must be changed as well if kKernelSize changes
    static const float kernel[kKernelSize * 2] = {
        0.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, -1.0f,
        -1.0f, 0.0f
    };
    enable();
    for (size_t i = 0; i < kKernelSize; i++)
        gl::Uniform2f(m_kernelLocation[i], kernel[2 * i + 0], kernel[2 * i + 1]);

    return true;
}

void ssaoMethod::setOccluderBias(float bias) {
    gl::Uniform1f(m_occluderBiasLocation, bias);
}

void ssaoMethod::setSamplingRadius(float radius) {
    gl::Uniform1f(m_samplingRadiusLocation, radius);
}

void ssaoMethod::setAttenuation(float constant, float linear) {
    gl::Uniform2f(m_attenuationLocation, constant, linear);
}

void ssaoMethod::setInverse(const m::mat4 &inverse) {
    // This is the inverse projection matrix used to reconstruct position from
    // depth in the SSAO pass
    gl::UniformMatrix4fv(m_inverseLocation, 1, GL_TRUE, (const GLfloat *)inverse.m);
}

void ssaoMethod::setWVP(const m::mat4 &wvp) {
    // Will set an identity matrix as this will be a screen space QUAD
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void ssaoMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenFrustumLocation, p.nearp, p.farp);
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
}

void ssaoMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureLocation, unit);
}

void ssaoMethod::setDepthTextureUnit(int unit) {
    gl::Uniform1i(m_depthTextureLocation, unit);
}

void ssaoMethod::setRandomTextureUnit(int unit) {
    gl::Uniform1i(m_randomTextureLocation, unit);
}

ssao::ssao()
    : m_fbo(0)
    , m_width(0)
    , m_height(0)
{
    m_textures[kBuffer] = 0;
    m_textures[kRandom] = 0;
}

ssao::~ssao() {
    destroy();
}

bool ssao::init(const m::perspective &p) {
    m_width = p.width / 2;
    m_height = p.height / 2;

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::GenTextures(2, m_textures);

    // 16-bit RED
    gl::BindTexture(format, m_textures[kBuffer]);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameterf(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexImage2D(format, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);

    // 8-bit RED random texture
    u::vector<unsigned char> random;
    random.reserve(kRandomSize * kRandomSize);
    for (size_t i = 0; i < kRandomSize; i++)
        for (size_t j = 0; j < kRandomSize; j++)
            random.push_back(u::randu() % 0xFF);

    gl::BindTexture(format, m_textures[kRandom]);
    gl::TexImage2D(format, 0, GL_R8, kRandomSize, kRandomSize, 0, GL_RED, GL_UNSIGNED_BYTE, &random[0]);

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_textures[kBuffer], 0);

    static GLuint drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

    gl::DrawBuffers(1, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void ssao::update(const m::perspective &p) {
    size_t width = p.width / 2;
    size_t height = p.height / 2;

    if (m_width != width || m_height != height) {
        GLenum format = gl::has(gl::ARB_texture_rectangle)
            ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
        m_width = width;
        m_height = height;
        gl::BindTexture(format, m_textures[kBuffer]);
        gl::TexImage2D(format, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
    }
}

void ssao::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_textures[0])
        gl::DeleteTextures(2, m_textures);
}

void ssao::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint ssao::texture(textureType type) const {
    return m_textures[type];
}

}
