#include "engine.h"

#include "r_aa.h"

#include "m_mat.h"

#include "u_file.h"
#include "u_misc.h"

namespace r {

/// FXAA: Fast Approximate Antialiasing

///! fxaaMethod
bool fxaaMethod::init(const u::initializer_list<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    if (!addShader(GL_VERTEX_SHADER, "shaders/fxaa.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/fxaa.fs"))
        return false;
    if (!finalize({ { 0, "position"  } },
                  { { 0, "fragColor" } }))
    {
        return false;
    }

    m_WVPLocation = getUniformLocation("gWVP");
    m_colorMapLocation = getUniformLocation("gColorMap");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    return true;
}

void fxaaMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void fxaaMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

void fxaaMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
    // TODO: frustum in final shader to do other things eventually
    //gl::Uniform2f(m_screenFrustumLocation, project.nearp, perspective.farp);
}

///! aa
fxaa::fxaa()
    : m_fbo(0)
    , m_texture(0)
    , m_width(0)
    , m_height(0)
{
}

fxaa::~fxaa() {
    destroy();
}

void fxaa::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
}

void fxaa::update(const m::perspective &p) {
    const size_t width = p.width;
    const size_t height = p.height;

    if (m_width == width && m_height == height)
        return;

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;
    m_width = width;
    m_height = height;

    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
}

bool fxaa::init(const m::perspective &p) {
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(1, &m_texture);

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_texture, 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

    gl::DrawBuffers(sizeof(drawBuffers)/sizeof(*drawBuffers), drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void fxaa::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint fxaa::texture() const {
    return m_texture;
}

/// SMAA: Enhanced Subpixel Morphological Antialiasing

///! smaaMethod
bool smaaMethod::init(const char *vertex, const char *fragment, const u::initializer_list<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    method::define("SMAA_GLSL_3 1");

    if (!addShader(GL_VERTEX_SHADER, vertex))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, fragment))
        return false;

    if (!finalize({ { 0, "position" },
                    { 1, "texCoord" } },
                  { { 0, "fragColor" } }))
    {
        return false;
    }

    m_WVPLocation = getUniformLocation("gWVP");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    return true;
}

void smaaMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void smaaMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
    // TODO: frustum in final shader to do other things eventually
    //gl::Uniform2f(m_screenFrustumLocation, project.nearp, perspective.farp);
}

///! smaaEdgeMethod
bool smaaEdgeMethod::init(const u::initializer_list<const char *> &defines) {
    if (!smaaMethod::init("shaders/smaa_edge.vs", "shaders/smaa_edge.fs", defines))
        return false;
    m_albedoMapLocation = getUniformLocation("gAlbedoMap");
    return true;
}

void smaaEdgeMethod::setAlbedoTextureUnit(int unit) {
    gl::Uniform1i(m_albedoMapLocation, unit);
}

///! smaaBlendMethod
bool smaaBlendMethod::init(const u::initializer_list<const char *> &defines) {
    if (!smaaMethod::init("shaders/smaa_blend.vs", "shaders/smaa_edge.fs", defines))
        return false;
    m_edgeMapLocation = getUniformLocation("gEdgeMap");
    m_areaMapLocation = getUniformLocation("gAreaMap");
    m_searchMapLocation = getUniformLocation("gSearchMap");
    return true;
}

void smaaBlendMethod::setEdgeTextureUnit(int unit) {
    gl::Uniform1i(m_edgeMapLocation, unit);
}

void smaaBlendMethod::setAreaTextureUnit(int unit) {
    gl::Uniform1i(m_areaMapLocation, unit);
}

void smaaBlendMethod::setSearchTextureUnit(int unit) {
    gl::Uniform1i(m_searchMapLocation, unit);
}

///! smaaNeighborMethod
bool smaaNeighborMethod::init(const u::initializer_list<const char *> &defines) {
    if (!smaaMethod::init("shaders/smaa_neighbor.vs", "shaders/smaa_neighbor.fs", defines))
        return false;
    m_albedoMapLocation = getUniformLocation("gAlbedoMap");
    m_blendMapLocation = getUniformLocation("gBlendMap");
    return true;
}

void smaaNeighborMethod::setAlbedoTextureUnit(int unit) {
    gl::Uniform1i(m_albedoMapLocation, unit);
}

void smaaNeighborMethod::setBlendTextureUnit(int unit) {
    gl::Uniform1i(m_blendMapLocation, unit);
}

///! smaaMethods
smaaMethods::smaaMethods()
    : m_initialized(false)
{
}

smaaMethods &smaaMethods::instance() {
    return m_instance;
}

bool smaaMethods::init() {
    if (m_initialized)
        return true;

    // must be in the same order as the enum
    static constexpr const char *kMethods[] = {
        "SMAA_PRESET_LOW 1",
        "SMAA_PRESET_MEDIUM 1",
        "SMAA_PRESET_HIGH 1",
        "SMAA_PRESET_ULTRA 1"
    };

    static constexpr size_t kCount = sizeof(kMethods)/sizeof(*kMethods);

    m_edgeMethods.resize(kCount);
    m_blendMethods.resize(kCount);
    m_neighborMethods.resize(kCount);

    for (size_t i = 0; i < kCount; i++) {
        if (!m_edgeMethods[i].init({kMethods[i]}))
            return false;
        m_edgeMethods[i].enable();
        m_edgeMethods[i].setAlbedoTextureUnit(0);

        if (!m_blendMethods[i].init({kMethods[i]}))
            return false;
        m_blendMethods[i].enable();
        m_blendMethods[i].setEdgeTextureUnit(0);
        m_blendMethods[i].setAreaTextureUnit(1);
        m_blendMethods[i].setSearchTextureUnit(2);

        if (!m_neighborMethods[i].init({kMethods[i]}))
            return false;
        m_neighborMethods[i].enable();
        m_neighborMethods[i].setAlbedoTextureUnit(0);
        m_neighborMethods[i].setBlendTextureUnit(1);
    }

    return m_initialized = true;
}

smaaEdgeMethod &smaaMethods::edge(quality q) {
    return m_edgeMethods[q];
}

smaaBlendMethod &smaaMethods::blend(quality q) {
    return m_blendMethods[q];
}

smaaNeighborMethod &smaaMethods::neighbor(quality q) {
    return m_neighborMethods[q];
}

smaaMethods smaaMethods::m_instance;

///! smaaData
static constexpr size_t kAreaWidth = 160;
static constexpr size_t kAreaHeight = 560;
static constexpr size_t kSearchWidth = 66;
static constexpr size_t kSearchHeight = 33;

// singleton to represent the area and search data required for smaa
struct smaaData {
    static smaaData &instance();
    bool init();
    const unsigned char *area() const;
    const unsigned char *search() const;
private:
    smaaData();
    smaaData(const smaaData &) = delete;
    void operator =(const smaaData &) = delete;
    bool m_initialized;
    u::vector<unsigned char> m_data;
    static smaaData m_instance;
};

smaaData &smaaData::instance() {
    return m_instance;
}

smaaData::smaaData()
    : m_initialized(false)
{
}

bool smaaData::init() {
    if (m_initialized)
        return true;
    auto data = u::read(u::format("%s%csmaa.bin", neoGamePath(), u::kPathSep));
    if (!data)
        return false;
    m_data = u::move(*data);
    // area is RG8, search is R8, therefor the final size of the data should be:
    // (kAreaWidth * kAreaHeight * 2) + (kSearchWidth * kSearchHeight)
    static constexpr size_t kRequired = (kAreaWidth * kAreaHeight * 2) + (kSearchWidth * kSearchHeight);
    if (m_data.size() != kRequired)
        return false;
    return m_initialized = true;
}

const unsigned char *smaaData::area() const {
    return &m_data[0];
}

const unsigned char *smaaData::search() const {
    // search data exists right after area data
    return &m_data[kAreaWidth * kAreaHeight * 2];
}

smaaData smaaData::m_instance;

///! smaa
smaa::smaa()
    : m_width(0)
    , m_height(0)
{
    memset(m_fbos, 0, sizeof(m_fbos));
    memset(m_textures, 0, sizeof(m_textures));
}

smaa::~smaa() {
    if (m_fbos[0])
        gl::DeleteFramebuffers(kCountFBO, m_fbos);
    if (m_textures[0])
        gl::DeleteTextures(kCountTex, m_textures);
}

bool smaa::init(const m::perspective &p) {
    m_width = p.width;
    m_height = p.height;

    smaaData &data = smaaData::instance();
    if (!data.init())
        return false;

    // edge, blend
    gl::GenTextures(kCountTex, m_textures);
    for (size_t i = 0; i != kAreaTex; i++) {
        gl::BindTexture(GL_TEXTURE_2D, m_textures[i]);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
            GL_FLOAT, nullptr);
    }

    // area
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kAreaTex]);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RG8, kAreaWidth, kAreaHeight, 0, GL_RG,
        GL_UNSIGNED_BYTE, data.area());

    // search
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kSearchTex]);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_R8, kSearchWidth, kSearchHeight, 0, GL_RED,
        GL_UNSIGNED_BYTE, data.search());

    static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    gl::GenFramebuffers(kCountFBO, m_fbos);
    for (size_t i = 0; i < kCountFBO; i++) {
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbos[i]);
        gl::DrawBuffers(1, drawBuffers);
        gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[i], 0);
        GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
            return false;
    }
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void smaa::update(const m::perspective &p) {
    const size_t width = p.width;
    const size_t height = p.height;

    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    for (size_t i = 0; i != kAreaTex; i++) {
        gl::BindTexture(GL_TEXTURE_2D, m_textures[i]);
        gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
                    GL_FLOAT, nullptr);
    }
}

void smaa::bindWriting(int fbo) {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbos[fbo]);
}

GLuint smaa::texture(int tex) {
    return m_textures[tex];
}

}
