#include "r_ssao.h"

#include "m_trig.h"

#include "u_vector.h"
#include "u_misc.h"

namespace r {

bool ssaoMethod::init(const u::vector<const char *> &defines) {
    if (!method::init("screen space ambient occlusion"))
        return false;

    for (const auto &it : defines)
        method::define(it);

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    method::define("kKernelSize", kKernelSize);
    method::define("kKernelFactor", m::sin(m::kPi / float(kKernelSize)));
    method::define("kKernelOffset", 1.0f / kKernelSize);

    if (!addShader(GL_VERTEX_SHADER, "shaders/ssao.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/ssao.fs"))
        return false;
    if (!finalize({ "position" }))
        return false;

    m_occluderBias = getUniform("gOccluderBias", uniform::kFloat);
    m_samplingRadius = getUniform("gSamplingRadius", uniform::kFloat);
    m_attenuation = getUniform("gAttenuation", uniform::kVec2);
    m_inverse = getUniform("gInverse", uniform::kMat4);
    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_screenFrustum = getUniform("gScreenFrustum", uniform::kVec2);
    m_screenSize = getUniform("gScreenSize", uniform::kVec2);
    m_normalTexture = getUniform("gNormalMap", uniform::kSampler);
    m_depthTexture = getUniform("gDepthMap", uniform::kSampler);
    m_randomTexture = getUniform("gRandomMap", uniform::kSampler);

    for (size_t i = 0; i < kKernelSize; i++)
        m_kernel[i] = getUniform(u::format("gKernel[%zu]", i), uniform::kVec2);

    post();

    // Setup the kernel
    // Note: This must be changed as well if kKernelSize changes
    static const m::vec2 kKernel[kKernelSize] = {
        {  0.0f,  1.0f, },
        {  1.0f,  0.0f, },
        {  0.0f, -1.0f, },
        { -1.0f,  0.0f  }
    };
    enable();
    for (const auto &it : kKernel)
        m_kernel[&it - kKernel]->set(it);

    return true;
}

void ssaoMethod::setOccluderBias(float bias) {
    m_occluderBias->set(bias);
}

void ssaoMethod::setSamplingRadius(float radius) {
    m_samplingRadius->set(radius);
}

void ssaoMethod::setAttenuation(float constant, float linear) {
    m_attenuation->set(m::vec2(constant, linear));
}

void ssaoMethod::setInverse(const m::mat4 &inverse) {
    m_inverse->set(inverse);
}

void ssaoMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void ssaoMethod::setPerspective(const m::perspective &p) {
    m_screenFrustum->set(m::vec2(p.nearp, p.farp));
    m_screenSize->set(m::vec2(p.width, p.height));
}

void ssaoMethod::setNormalTextureUnit(int unit) {
    m_normalTexture->set(unit);
}

void ssaoMethod::setDepthTextureUnit(int unit) {
    m_depthTexture->set(unit);
}

void ssaoMethod::setRandomTextureUnit(int unit) {
    m_randomTexture->set(unit);
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

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

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

    const GLenum status = gl::CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void ssao::update(const m::perspective &p) {
    const size_t width = p.width / 2;
    const size_t height = p.height / 2;

    if (m_width == width && m_height == height)
        return;

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    m_width = width;
    m_height = height;
    gl::BindTexture(format, m_textures[kBuffer]);
    gl::TexImage2D(format, 0, GL_R16F, m_width, m_height, 0, GL_RED, GL_FLOAT, nullptr);
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
