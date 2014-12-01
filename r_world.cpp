#include "r_world.h"
#include "r_model.h"

#include "u_algorithm.h"
#include "u_memory.h"
#include "u_misc.h"
#include "u_file.h"

#include "c_var.h"

#include "engine.h"

VAR(int, r_fxaa, "fast approximate anti-aliasing", 0, 1, 1);
VAR(int, r_parallax, "parallax mapping", 0, 1, 1);
VAR(int, r_ssao, "screen space ambient occlusion", 0, 1, 1);

namespace r {
///! methods

///! Light Rendering Method
bool lightMethod::init(const char *vs, const char *fs, const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    if (gl::has(ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, vs))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, fs))
        return false;
    if (!finalize())
        return false;

    // matrices
    m_WVPLocation = getUniformLocation("gWVP");
    m_inverseLocation = getUniformLocation("gInverse");

    // samplers
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");
    m_normalTextureUnitLocation = getUniformLocation("gNormalMap");
    m_occlusionTextureUnitLocation = getUniformLocation("gOcclusionMap");
    m_depthTextureUnitLocation = getUniformLocation("gDepthMap");

    // specular lighting
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");

    // device uniforms
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_screenFrustumLocation = getUniformLocation("gScreenFrustum");

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat*)wvp.m);
}

void lightMethod::setInverse(const m::mat4 &inverse) {
    gl::UniformMatrix4fv(m_inverseLocation, 1, GL_TRUE, (const GLfloat*)inverse.m);
}

void lightMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void lightMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureUnitLocation, unit);
}

void lightMethod::setDepthTextureUnit(int unit) {
    gl::Uniform1i(m_depthTextureUnitLocation, unit);
}

void lightMethod::setOcclusionTextureUnit(int unit) {
    gl::Uniform1i(m_occlusionTextureUnitLocation, unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, &position.x);
}

void lightMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
    gl::Uniform2f(m_screenFrustumLocation, project.nearp, project.farp);
}

///! Directional Light Rendering Method
bool directionalLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/dlight.vs", "shaders/dlight.fs", defines))
        return false;

    m_directionalLightLocation.color = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLightLocation.ambient = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLightLocation.diffuse = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLightLocation.direction = getUniformLocation("gDirectionalLight.direction");

    return true;
}

void directionalLightMethod::setLight(const directionalLight &light) {
    m::vec3 direction = light.direction.normalized();
    gl::Uniform3fv(m_directionalLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_directionalLightLocation.ambient, light.ambient);
    gl::Uniform3fv(m_directionalLightLocation.direction, 1, &direction.x);
    gl::Uniform1f(m_directionalLightLocation.diffuse, light.diffuse);
}

///! Point Light Rendering Method
bool pointLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/plight.vs", "shaders/plight.fs", defines))
        return false;

    m_pointLightLocation.color = getUniformLocation("gPointLight.base.color");
    m_pointLightLocation.ambient = getUniformLocation("gPointLight.base.ambient");
    m_pointLightLocation.diffuse = getUniformLocation("gPointLight.base.diffuse");
    m_pointLightLocation.position = getUniformLocation("gPointLight.position");
    m_pointLightLocation.radius = getUniformLocation("gPointLight.radius");

    return true;
}

void pointLightMethod::setLight(const pointLight &light) {
    gl::Uniform3fv(m_pointLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_pointLightLocation.ambient, light.ambient);
    gl::Uniform1f(m_pointLightLocation.diffuse, light.diffuse);
    gl::Uniform3fv(m_pointLightLocation.position, 1, &light.position.x);
    gl::Uniform1f(m_pointLightLocation.radius, light.radius);
}

///! Spot Light Rendering Method
bool spotLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/slight.vs", "shaders/slight.fs", defines))
        return false;

    m_spotLightLocation.color = getUniformLocation("gSpotLight.base.base.color");
    m_spotLightLocation.ambient = getUniformLocation("gSpotLight.base.base.ambient");
    m_spotLightLocation.diffuse = getUniformLocation("gSpotLight.base.base.diffuse");
    m_spotLightLocation.position = getUniformLocation("gSpotLight.base.position");
    m_spotLightLocation.radius = getUniformLocation("gSpotLight.base.radius");
    m_spotLightLocation.direction = getUniformLocation("gSpotLight.direction");
    m_spotLightLocation.cutOff = getUniformLocation("gSpotLight.cutOfff");

    return true;
}

void spotLightMethod::setLight(const spotLight &light) {
    gl::Uniform3fv(m_spotLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_spotLightLocation.ambient, light.ambient);
    gl::Uniform1f(m_spotLightLocation.diffuse, light.diffuse);
    gl::Uniform3fv(m_spotLightLocation.position, 1, &light.position.x);
    gl::Uniform1f(m_spotLightLocation.radius, light.radius);
    gl::Uniform3fv(m_spotLightLocation.direction, 1, &light.direction.x);
    gl::Uniform1f(m_spotLightLocation.cutOff, light.cutOff);
}

///! Geomety Rendering Method
bool geomMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/geom.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/geom.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");
    m_normalTextureUnitLocation = getUniformLocation("gNormalMap");
    m_specTextureUnitLocation = getUniformLocation("gSpecMap");
    m_dispTextureUnitLocation = getUniformLocation("gDispMap");
    m_specPowerLocation = getUniformLocation("gSpecPower");
    m_specIntensityLocation = getUniformLocation("gSpecIntensity");
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");
    m_parallaxLocation = getUniformLocation("gParallax");

    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void geomMethod::setEyeWorldPos(const m::vec3 &position) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, &position.x);
}

void geomMethod::setParallax(float scale, float bias) {
    gl::Uniform2f(m_parallaxLocation, scale, bias);
}

void geomMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void geomMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureUnitLocation, unit);
}

void geomMethod::setDispTextureUnit(int unit) {
    gl::Uniform1i(m_dispTextureUnitLocation, unit);
}

void geomMethod::setSpecTextureUnit(int unit) {
    gl::Uniform1i(m_specTextureUnitLocation, unit);
}

void geomMethod::setSpecIntensity(float intensity) {
    gl::Uniform1f(m_specIntensityLocation, intensity);
}

void geomMethod::setSpecPower(float power) {
    gl::Uniform1f(m_specPowerLocation, power);
}

///! SSAO Method
bool ssaoMethod::init() {
    if (!method::init())
        return false;

    if (gl::has(ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    method::define("kKernelSize", kKernelSize);
    method::define("kKernelFactor", sinf(m::kPi / float(kKernelSize)));
    method::define("kKernelOffset", 1.0f / kKernelSize);

    if (!addShader(GL_VERTEX_SHADER, "shaders/ssao.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/ssao.fs"))
        return false;
    if (!finalize())
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

void ssaoMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenFrustumLocation, project.nearp, project.farp);
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
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

///! Final Composite Method
bool finalMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (gl::has(ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    if (!addShader(GL_VERTEX_SHADER, "shaders/final.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/final.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_colorMapLocation = getUniformLocation("gColorMap");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    return true;
}

void finalMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void finalMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

void finalMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
    // TODO: frustum in final shader to do other things eventually
    //gl::Uniform2f(m_screenFrustumLocation, project.nearp, project.farp);
}

finalComposite::finalComposite()
    : m_fbo(0)
    , m_texture(0)
    , m_width(0)
    , m_height(0)
{
}

finalComposite::~finalComposite() {
    destroy();
}

void finalComposite::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
}

void finalComposite::update(const m::perspectiveProjection &project, GLuint depth) {
    size_t width = project.width;
    size_t height = project.height;

    if (m_width != width || m_height != height) {
        destroy();
        init(project, depth);
    }
}

bool finalComposite::init(const m::perspectiveProjection &project, GLuint depth) {
    m_width = project.width;
    m_height = project.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(1, &m_texture);

    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // final composite
    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format, m_texture, 0);

    gl::BindTexture(format, depth);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, format, depth, 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    gl::DrawBuffers(1, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void finalComposite::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint finalComposite::texture() const {
    return m_texture;
}

///! renderer
world::world()
    : m_uploaded(false)
{
    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.50f;
    m_directionalLight.diffuse = 0.75f;

    const m::vec3 places[] = {
        m::vec3(153.04, 105.02, 197.67),
        m::vec3(-64.14, 105.02, 328.36),
        m::vec3(-279.83, 105.02, 204.61),
        m::vec3(-458.72, 101.02, 189.58),
        m::vec3(-664.53, 75.02, -1.75),
        m::vec3(-580.69, 68.02, -184.89),
        m::vec3(-104.43, 84.02, -292.99),
        m::vec3(-23.59, 84.02, -292.40),
        m::vec3(333.00, 101.02, 194.46),
        m::vec3(167.13, 101.02, 0.32),
        m::vec3(-63.36, 37.20, 2.30),
        m::vec3(459.97, 68.02, -181.60),
        m::vec3(536.75, 75.01, 2.80),
        m::vec3(-4.61, 117.02, -91.74),
        m::vec3(-2.33, 117.02, 86.34),
        m::vec3(-122.92, 117.02, 84.58),
        m::vec3(-123.44, 117.02, -86.57),
        m::vec3(-300.24, 101.02, -0.15),
        m::vec3(-448.34, 101.02, -156.27),
        m::vec3(-452.94, 101.02, 23.58),
        m::vec3(-206.59, 101.02, -209.52),
        m::vec3(62.59, 101.02, -207.53)
    };

    m_billboards.resize(kBillboardMax);

    pointLight light;
    light.diffuse = 1.0f;
    light.ambient = 1.0f;
    light.radius = 30.0f;
    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        switch (rand() % 4) {
            case 0: m_billboards[kBillboardRail].add(places[i]); break;
            case 1: m_billboards[kBillboardLightning].add(places[i]); break;
            case 2: m_billboards[kBillboardRocket].add(places[i]); break;
            case 3: m_billboards[kBillboardShotgun].add(places[i]); break;
        }
        light.color = { float(rand()) / float(RAND_MAX),
                        float(rand()) / float(RAND_MAX),
                        float(rand()) / float(RAND_MAX) };
        light.position = places[i];
        light.position.y -= 10.0f;
        m_pointLights.push_back(light);
    }

    light.position = { 0, 110, 0 };
    m_pointLights.push_back(light);
}

world::~world() {
    gl::DeleteBuffers(2, m_buffers);
    gl::DeleteVertexArrays(1, &m_vao);

    for (auto &it : m_textures2D)
        delete it.second;
}

/// shader permutations
struct geomPermutation {
    int permute;    // flags of the permutations
    int color;      // color texture unit (or -1 if not to be used)
    int normal;     // normal texture unit (or -1 if not to be used)
    int spec;       // ...
    int disp;       // ...
};

struct finalPermutation {
    int permute;    // flags of the permutations
};

struct lightPermutation {
    int permute;    // flags of the permutations
};

// All the final shader permutations
enum {
    kFinalPermFXAA       = 1 << 0
};

// All the light shader permutations
enum {
    kLightPermSSAO       = 1 << 0
};

// All the geometry shader permutations
enum {
    kGeomPermDiffuse     = 1 << 0,
    kGeomPermNormalMap   = 1 << 1,
    kGeomPermSpecMap     = 1 << 2,
    kGeomPermSpecParams  = 1 << 3,
    kGeomPermParallax    = 1 << 4
};

// The prelude defines to compile that final shader permutation
// These must be in the same order as the enums
static const char *finalPermutationNames[] = {
    "USE_FXAA"
};

// The prelude defines to compile that light shader permutation
// These must be in the same order as the enums
static const char *lightPermutationNames[] = {
    "USE_SSAO"
};

// The prelude defines to compile that geometry shader permutation
// These must be in the same order as the enums
static const char *geomPermutationNames[] = {
    "USE_DIFFUSE",
    "USE_NORMALMAP",
    "USE_SPECMAP",
    "USE_SPECPARAMS",
    "USE_PARALLAX"
};

// All the possible final permutations
static const finalPermutation finalPermutations[] = {
    { 0 },
    { kFinalPermFXAA }
};

// All the possible light permutations
static const lightPermutation lightPermutations[] = {
    { 0 },
    { kLightPermSSAO }
};

// All the possible geometry permutations
static const geomPermutation geomPermutations[] = {
    { 0,                                                                              -1, -1, -1, -1 },
    { kGeomPermDiffuse,                                                                0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                           0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap,                                             0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams,                                          0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                           0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap | kGeomPermSpecMap,                        0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap | kGeomPermSpecParams,                     0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap | kGeomPermParallax,                       0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap | kGeomPermSpecMap    | kGeomPermParallax, 0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap | kGeomPermSpecParams | kGeomPermParallax, 0,  1, -1,  2 }
};

// Generate the list of permutation names for the shader
template <typename T, size_t N, typename U>
static u::vector<const char *> generatePermutation(const T(&list)[N], const U &p) {
    u::vector<const char *> permutes;
    for (size_t i = 0; i < N; i++)
        if (p.permute & (1 << i))
            permutes.push_back(list[i]);
    return permutes;
}

// Calculate the correct permutation to use for the final composite
static size_t finalCalculatePermutation() {
    for (size_t i = 0; i < sizeof(geomPermutations)/sizeof(geomPermutations[0]); i++)
        if (r_fxaa && (geomPermutations[i].permute & kFinalPermFXAA))
            return i;
    return 0;
}

// Calculate the correct permutation to use for the light buffer
static size_t lightCalculatePermutation() {
    for (size_t i = 0; i < sizeof(lightPermutations)/sizeof(lightPermutations[0]); i++)
        if (r_ssao && (lightPermutations[i].permute & kLightPermSSAO))
            return i;
    return 0;
}

// Calculate the correct permutation to use for a given material
static void geomCalculatePermutation(material &mat) {
    int permute = 0;
    if (mat.diffuse)
        permute |= kGeomPermDiffuse;
    if (mat.normal)
        permute |= kGeomPermNormalMap;
    if (mat.spec)
        permute |= kGeomPermSpecMap;
    if (mat.displacement && r_parallax)
        permute |= kGeomPermParallax;
    if (mat.specParams)
        permute |= kGeomPermSpecParams;
    for (size_t i = 0; i < sizeof(geomPermutations)/sizeof(geomPermutations[0]); i++) {
        if (geomPermutations[i].permute == permute) {
            mat.permute = i;
            return;
        }
    }
}

bool world::load(const kdMap &map) {
    // load skybox
    if (!m_skybox.load("textures/sky01"))
        return false;

    // Load a model
    m_models.resize(1);
    if (!m_models[0].load(m_textures2D, "models/test"))
        return false;
    // Figure out the model's material's permutation
    geomCalculatePermutation(m_models[0].mat);

    static const struct {
        const char *name;
        const char *file;
        billboardType type;
    } billboards[] = {
        { "railgun",         "textures/railgun",   kBillboardRail },
        { "lightning gun",   "textures/lightgun",  kBillboardLightning },
        { "rocket launcher", "textures/rocketgun", kBillboardRocket },
        { "shotgun",         "textures/shotgun",   kBillboardShotgun }
    };

    for (size_t i = 0; i < sizeof(billboards)/sizeof(*billboards); i++) {
        auto &billboard = billboards[i];
        if (m_billboards[billboard.type].load(billboard.file))
            continue;
        neoFatal("failed to load billboard for `%s'", billboard.name);
    }

    // make rendering batches for triangles which share the same texture
    for (size_t i = 0; i < map.textures.size(); i++) {
        renderTextureBatch batch;
        batch.start = m_indices.size();
        batch.index = i;
        for (size_t j = 0; j < map.triangles.size(); j++) {
            if (map.triangles[j].texture == i)
                for (size_t k = 0; k < 3; k++)
                    m_indices.push_back(map.triangles[j].v[k]);
        }
        batch.count = m_indices.size() - batch.start;
        m_textureBatches.push_back(batch);
    }

// TODO: Offline step in kdtree.cpp instead
#if 0
    u::print("Optimizing world geometry (this could take awhile)\n");
    // optimize the indices
    size_t cacheMissesBefore = 0;
    size_t cacheMissesAfter = 0;
    for (auto &it : m_textureBatches) {
        // collect the indices to optimize
        u::vector<size_t> indices;
        indices.resize(it.count);
        for (size_t i = 0; i < it.count; i++)
            indices[i] = m_indices[it.start + i];

        // Calculate the number of cache misses in the unoptimized mesh
        vertexCache cache;
        cacheMissesBefore += cache.getCacheMissCount(indices);

        // Optimize the indices
        vertexCacheOptimizer opt;
        auto result = opt.optimize(indices);
        if (result == vertexCacheOptimizer::kSuccess) {
            cacheMissesAfter += opt.getCacheMissCount();
            for (size_t i = 0; i < it.count; i++)
                m_indices[it.start + i] = indices[i];
        }
    }
    u::print("Eliminated %zu cache misses\n", cacheMissesBefore - cacheMissesAfter);
#endif

    // load materials
    for (auto &it : m_textureBatches) {
        if (!it.mat.load(m_textures2D, map.textures[it.index].name, "textures/"))
            neoFatal("failed to load material\n");
        geomCalculatePermutation(it.mat);
    }

    m_vertices = u::move(map.vertices);
    u::print("[world] => loaded\n");
    return true;
}

bool world::upload(const m::perspectiveProjection &project) {
    if (m_uploaded)
        return true;

    m_identity.loadIdentity();

    // upload skybox
    if (!m_skybox.upload())
        neoFatal("failed to upload skybox");
    if (!m_quad.upload())
        neoFatal("failed to upload quad");
    if (!m_sphere.upload())
        neoFatal("failed to upload sphere");

    // upload the model
    if (!m_models[0].upload())
        neoFatal("failed to upload model");
    else
        u::print("[model] => loaded %s\n", m_models[0].name());

    // upload billboards
    for (auto &it : m_billboards) {
        if (it.upload())
            continue;
        neoFatal("failed to upload billboard");
    }

    // upload materials
    for (auto &it : m_textureBatches)
        if (!it.mat.upload())
            neoFatal("failed to upload world materials");

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(kdBinVertex), &m_vertices[0], GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(0));  // vertex
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(3));  // normals
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(6));  // texCoord
    gl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(8));  // tangent
    gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(11)); // w
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);
    gl::EnableVertexAttribArray(3);
    gl::EnableVertexAttribArray(4);

    // upload data
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_STATIC_DRAW);

    // final shader permutations
    static const size_t finalCount = sizeof(finalPermutations)/sizeof(finalPermutations[0]);
    m_finalMethods.resize(finalCount);
    for (size_t i = 0; i < finalCount; i++) {
        const auto &p = finalPermutations[i];
        if (!m_finalMethods[i].init(generatePermutation(finalPermutationNames, p)))
            neoFatal("failed to initialize final composite rendering method");
        m_finalMethods[i].enable();
        m_finalMethods[i].setColorTextureUnit(0);
        m_finalMethods[i].setWVP(m_identity);
    }

    // geometry shader permutations
    static const size_t geomCount = sizeof(geomPermutations)/sizeof(geomPermutations[0]);
    m_geomMethods.resize(geomCount);
    for (size_t i = 0; i < geomCount; i++) {
        const auto &p = geomPermutations[i];
        if (!m_geomMethods[i].init(generatePermutation(geomPermutationNames, p)))
            neoFatal("failed to initialize geometry rendering method");
        m_geomMethods[i].enable();
        if (p.color  != -1) m_geomMethods[i].setColorTextureUnit(p.color);
        if (p.normal != -1) m_geomMethods[i].setNormalTextureUnit(p.normal);
        if (p.spec   != -1) m_geomMethods[i].setSpecTextureUnit(p.spec);
        if (p.disp   != -1) m_geomMethods[i].setDispTextureUnit(p.disp);
    }

    // directional light shader permutations
    static const size_t lightCount = sizeof(lightPermutations)/sizeof(lightPermutations[0]);
    m_directionalLightMethods.resize(lightCount);
    for (size_t i = 0; i < lightCount; i++) {
        const auto &p = lightPermutations[i];
        if (!m_directionalLightMethods[i].init(generatePermutation(lightPermutationNames, p)))
            neoFatal("failed to initialize light rendering method");
        m_directionalLightMethods[i].enable();
        m_directionalLightMethods[i].setWVP(m_identity);
        m_directionalLightMethods[i].setColorTextureUnit(lightMethod::kColor);
        m_directionalLightMethods[i].setNormalTextureUnit(lightMethod::kNormal);
        m_directionalLightMethods[i].setDepthTextureUnit(lightMethod::kDepth);
        if (p.permute & kLightPermSSAO)
            m_directionalLightMethods[i].setOcclusionTextureUnit(lightMethod::kOcclusion);
    }

    // point light method
    if (!m_pointLightMethod.init())
        neoFatal("failed to initialize point-light rendering method");
    m_pointLightMethod.enable();
    m_pointLightMethod.setColorTextureUnit(lightMethod::kColor);
    m_pointLightMethod.setNormalTextureUnit(lightMethod::kNormal);
    m_pointLightMethod.setDepthTextureUnit(lightMethod::kDepth);

    // spot light method
    if (!m_spotLightMethod.init())
        neoFatal("failed to initialize spot-light rendering method");
    m_spotLightMethod.enable();
    m_spotLightMethod.setColorTextureUnit(lightMethod::kColor);
    m_spotLightMethod.setNormalTextureUnit(lightMethod::kNormal);
    m_spotLightMethod.setDepthTextureUnit(lightMethod::kDepth);

    // setup g-buffer
    if (!m_gBuffer.init(project))
        neoFatal("failed to initialize world renderer");
    if (!m_ssao.init(project))
        neoFatal("failed to initialize world renderer");
    if (!m_final.init(project, m_gBuffer.texture(gBuffer::kDepth)))
        neoFatal("failed to initialize world renderer");

    if (!m_ssaoMethod.init())
        neoFatal("failed to initialize ssao rendering method");

    // Setup default uniforms for ssao
    m_ssaoMethod.enable();
    m_ssaoMethod.setWVP(m_identity);
    m_ssaoMethod.setOccluderBias(0.05f);
    m_ssaoMethod.setSamplingRadius(15.0f);
    m_ssaoMethod.setAttenuation(1.0f, 0.000005f);
    m_ssaoMethod.setNormalTextureUnit(ssaoMethod::kNormal);
    m_ssaoMethod.setDepthTextureUnit(ssaoMethod::kDepth);
    m_ssaoMethod.setRandomTextureUnit(ssaoMethod::kRandom);

    u::print("[world] => uploaded\n");
    return m_uploaded = true;
}

void world::scenePass(const rendererPipeline &pipeline) {
    auto p = pipeline;

    // The scene pass will be writing into the gbuffer
    m_gBuffer.update(p.getPerspectiveProjection());
    m_gBuffer.bindWriting();

    // Clear the depth and color buffers. This is a new scene pass.
    // We need depth testing as the scene pass will write into the depth
    // buffer. Blending isn't needed.
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gl::Enable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);

    auto setup = [this](material &mat, rendererPipeline &p) {
        // Calculate permutation incase a console variable changes
        geomCalculatePermutation(mat);

        auto &permutation = geomPermutations[mat.permute];
        auto &method = m_geomMethods[mat.permute];
        method.enable();
        method.setWVP(p.getWVPTransform());
        method.setWorld(p.getWorldTransform());
        if (permutation.permute & kGeomPermParallax) {
            method.setEyeWorldPos(p.getPosition());
            method.setParallax(mat.dispScale, mat.dispBias);
        }
        if (permutation.permute & kGeomPermSpecParams) {
            method.setSpecIntensity(mat.specIntensity);
            method.setSpecPower(mat.specPower);
        }
        if (permutation.permute & kGeomPermDiffuse)
            mat.diffuse->bind(GL_TEXTURE0 + permutation.color);
        if (permutation.permute & kGeomPermNormalMap)
            mat.normal->bind(GL_TEXTURE0 + permutation.normal);
        if (permutation.permute & kGeomPermSpecMap)
            mat.spec->bind(GL_TEXTURE0 + permutation.spec);
        if (permutation.permute & kGeomPermParallax)
            mat.displacement->bind(GL_TEXTURE0 + permutation.disp);
    };

    // Render the map
    gl::BindVertexArray(m_vao);
    for (auto &it : m_textureBatches) {
        setup(it.mat, p);
        gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(GLuint) * it.start));
    }

    // Render map models
    for (auto &it : m_models) {
        // TODO: model properties adjust this
        p.setWorldPosition({0, 120, 0});
        p.setScale(it.scale * m::vec3(1.0f, 1.0f, -1.0f));
        p.setRotate(it.rotate);

        setup(it.mat, p);
        it.render();
    }

    // Only the scene pass needs to write to the depth buffer
    gl::Disable(GL_DEPTH_TEST);

    // Screen space ambient occlusion pass
    if (r_ssao) {
        // Read from the gbuffer, write to the ssao pass
        m_ssao.update(p.getPerspectiveProjection());
        m_ssao.bindWriting();

        // Write to SSAO
        GLenum format = gl::has(ARB_texture_rectangle)
            ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

        // Bind normal/depth/random
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kNormal);
        gl::BindTexture(format, m_gBuffer.texture(gBuffer::kNormal));
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kDepth);
        gl::BindTexture(format, m_gBuffer.texture(gBuffer::kDepth));
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kRandom);
        gl::BindTexture(format, m_ssao.texture(ssao::kRandom));

        // do SSAO pass
        m_ssaoMethod.enable();
        m_ssaoMethod.setPerspectiveProjection(p.getPerspectiveProjection());
        m_ssaoMethod.setInverse(p.getInverseTransform());
        m_quad.render();
    }
}

void world::lightPass(const rendererPipeline &pipeline) {
    auto p = pipeline;

    // Write to the final composite
    m_final.bindWriting();

    // Lighting will require blending
    gl::Enable(GL_BLEND);
    gl::BlendEquation(GL_FUNC_ADD);
    gl::BlendFunc(GL_ONE, GL_ONE);

    // Clear the final composite
    gl::Clear(GL_COLOR_BUFFER_BIT);

    // Need to read from the gbuffer and ssao buffer to do lighting
    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kColor);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kColor));
    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kNormal);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kNormal));
    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kDepth);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kDepth));
    if (r_ssao) {
        gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kOcclusion);
        gl::BindTexture(format, m_ssao.texture(ssao::kBuffer));
    }

    // Point Lighting
    {
        auto &method = m_pointLightMethod;
        method.enable();
        method.setPerspectiveProjection(pipeline.getPerspectiveProjection());
        method.setEyeWorldPos(pipeline.getPosition());
        method.setInverse(p.getInverseTransform());

        auto project = pipeline.getPerspectiveProjection();
        rendererPipeline p;
        p.setPosition(pipeline.getPosition());
        p.setRotation(pipeline.getRotation());
        p.setPerspectiveProjection(project);

        static constexpr float kLightRadiusTweak = 1.11f;
        gl::Enable(GL_DEPTH_TEST);
        gl::DepthMask(GL_FALSE);
        for (auto &it : m_pointLights) {
            float scale = it.radius * kLightRadiusTweak;

            // Frustum cull lights
            m_frustum.setup(it.position, p.getRotation(), p.getPerspectiveProjection());
            if (!m_frustum.testSphere(p.getPosition(), scale))
                continue;

            p.setWorldPosition(it.position);
            p.setScale({scale, scale, scale});

            method.setLight(it);
            method.setWVP(p.getWVPTransform());

            const m::vec3 dist = it.position - p.getPosition();
            scale += project.nearp + 1;
            if (dist*dist >= scale*scale) {
                gl::DepthFunc(GL_LESS);
                gl::CullFace(GL_BACK);
            } else {
                gl::DepthFunc(GL_GEQUAL);
                gl::CullFace(GL_FRONT);
            }
            m_sphere.render();
        }
        gl::DepthMask(GL_TRUE);
        gl::DepthFunc(GL_LESS);
        gl::CullFace(GL_BACK);
        gl::Disable(GL_DEPTH_TEST);
    }

    // Spot lighting
    {
        auto &method = m_spotLightMethod;
        method.enable();
        method.setPerspectiveProjection(pipeline.getPerspectiveProjection());
        method.setEyeWorldPos(pipeline.getPosition());
        method.setInverse(p.getInverseTransform());

        auto project = pipeline.getPerspectiveProjection();
        rendererPipeline p;
        p.setPosition(pipeline.getPosition());
        p.setRotation(pipeline.getRotation());
        p.setPerspectiveProjection(project);

        static constexpr float kLightRadiusTweak = 1.11f;
        gl::Enable(GL_DEPTH_TEST);
        gl::DepthMask(GL_FALSE);
        for (auto &it : m_spotLights) {
            float scale = it.radius * kLightRadiusTweak;

            // Frustum cull lights
            m_frustum.setup(it.position, p.getRotation(), p.getPerspectiveProjection());
            if (!m_frustum.testSphere(p.getPosition(), scale))
                continue;

            p.setWorldPosition(it.position);
            p.setScale({scale, scale, scale});

            method.setLight(it);
            method.setWVP(p.getWVPTransform());

            const m::vec3 dist = it.position - p.getPosition();
            scale += project.nearp + 1;
            if (dist*dist >= scale*scale) {
                gl::DepthFunc(GL_LESS);
                gl::CullFace(GL_BACK);
            } else {
                gl::DepthFunc(GL_GEQUAL);
                gl::CullFace(GL_FRONT);
            }
            m_sphere.render();
        }
        gl::DepthMask(GL_TRUE);
        gl::DepthFunc(GL_LESS);
        gl::CullFace(GL_BACK);
        gl::Disable(GL_DEPTH_TEST);
    }

    // Directional Lighting
    {
        auto &method = m_directionalLightMethods[lightCalculatePermutation()];
        method.enable();
        method.setLight(m_directionalLight);
        method.setPerspectiveProjection(pipeline.getPerspectiveProjection());
        method.setEyeWorldPos(pipeline.getPosition());
        method.setInverse(p.getInverseTransform());
        m_quad.render();
    }
}

void world::otherPass(const rendererPipeline &pipeline) {
    m_final.bindWriting();

    // Forward rendering takes place here, reenable depth testing
    gl::Enable(GL_DEPTH_TEST);

    // Forward render skybox
    m_skybox.render(pipeline);

    // World billboards
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto &it : m_billboards)
        it.render(pipeline);

    // Don't need depth testing or blending anymore
    gl::Disable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);
}

void world::finalPass(const rendererPipeline &pipeline) {
    // We're going to be reading from the final composite
    m_final.update(pipeline.getPerspectiveProjection(), m_gBuffer.texture(gBuffer::kDepth));

    // For the final pass it's important we output to the screen
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    GLenum format = gl::has(ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::ActiveTexture(GL_TEXTURE0);
    gl::BindTexture(format, m_final.texture());

    const size_t index = finalCalculatePermutation();
    auto &it = m_finalMethods[index];
    it.enable();
    it.setPerspectiveProjection(pipeline.getPerspectiveProjection());
    m_quad.render();
}

void world::render(const rendererPipeline &pipeline) {
    scenePass(pipeline);
    lightPass(pipeline);
    otherPass(pipeline);
    finalPass(pipeline);
}

}
