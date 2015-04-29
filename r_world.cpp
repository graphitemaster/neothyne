#include <assert.h>

#include "engine.h"
#include "world.h"
#include "cvar.h"
#include "gui.h"

#include "r_model.h"
#include "r_pipeline.h"

#include "u_algorithm.h"
#include "u_memory.h"
#include "u_misc.h"
#include "u_file.h"

// Debug visualizations
enum {
    kDebugDepth = 1,
    kDebugNormal,
    kDebugPosition,
    kDebugSSAO
};

VAR(int, r_fxaa, "fast approximate anti-aliasing", 0, 1, 1);
VAR(int, r_parallax, "parallax mapping", 0, 1, 1);
VAR(int, r_ssao, "screen space ambient occlusion", 0, 1, 1);
VAR(int, r_spec, "specularity mapping", 0, 1, 1);
VAR(int, r_hoq, "hardware occlusion queries", 0, 1, 1);
VAR(int, r_fog, "fog", 0, 1, 1);
NVAR(int, r_debug, "debug visualizations", 0, 4, 0);

namespace r {

/// shader permutations
struct geomPermutation {
    int permute;    // flags of the permutations
    int color;      // color texture unit (or -1 if not to be used)
    int normal;     // normal texture unit (or -1 if not to be used)
    int spec;       // ...
    int disp;       // ...
};

struct lightPermutation {
    int permute;    // flags of the permutations
};

// All the light shader permutations
enum {
    kLightPermSSAO          = 1 << 0,
    kLightPermFog           = 1 << 1,
    kLightPermDebugDepth    = 1 << 2,
    kLightPermDebugNormal   = 1 << 3,
    kLightPermDebugPosition = 1 << 4,
    kLightPermDebugSSAO     = 1 << 5
};

// All the geometry shader permutations
enum {
    kGeomPermDiffuse        = 1 << 0,
    kGeomPermNormalMap      = 1 << 1,
    kGeomPermSpecMap        = 1 << 2,
    kGeomPermSpecParams     = 1 << 3,
    kGeomPermParallax       = 1 << 4,
    kGeomPermSkeletal       = 1 << 5
};

// The prelude defines to compile that light shader permutation
// These must be in the same order as the enums
static const char *lightPermutationNames[] = {
    "USE_SSAO",
    "USE_FOG",
    "USE_DEBUG_DEPTH",
    "USE_DEBUG_NORMAL",
    "USE_DEBUG_POSITION",
    "USE_DEBUG_SSAO"
};

// The prelude defines to compile that geometry shader permutation
// These must be in the same order as the enums
static const char *geomPermutationNames[] = {
    "USE_DIFFUSE",
    "USE_NORMALMAP",
    "USE_SPECMAP",
    "USE_SPECPARAMS",
    "USE_PARALLAX",
    "USE_SKELETAL"
};

// All the possible light permutations
static const lightPermutation lightPermutations[] = {
    { 0 },
    { kLightPermSSAO },
    { kLightPermSSAO | kLightPermFog },
    { kLightPermFog },
    // Debug visualizations
    { kLightPermDebugDepth },
    { kLightPermDebugNormal },
    { kLightPermDebugPosition },
    { kLightPermDebugSSAO },
};

// All the possible geometry permutations
static const geomPermutation geomPermutations[] = {
    { 0,                                                                                                   -1, -1, -1, -1 },
    { kGeomPermDiffuse,                                                                                     0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                                                0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap,                                                                  0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams,                                                               0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                                                0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap,                                            0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams,                                         0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax,                                           0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax,                     0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax,                     0,  1, -1,  2 },
    { kGeomPermSkeletal,                                                                                   -1, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSkeletal,                                                                 0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal,                                           0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap    | kGeomPermSkeletal,                                           0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams | kGeomPermSkeletal,                                           0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal,                                           0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermSkeletal,                     0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermSkeletal,                     0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax   | kGeomPermSkeletal,                     0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax | kGeomPermSkeletal, 0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax | kGeomPermSkeletal, 0,  1, -1,  2 }
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

// Calculate the correct permutation to use for the light buffer
static size_t lightCalculatePermutation(bool stencil) {
    for (auto &it : lightPermutations) {
        switch (r_debug) {
        case kDebugDepth:
            if (it.permute == kLightPermDebugDepth)
                return &it - lightPermutations;
            break;
        case kDebugNormal:
            if (it.permute == kLightPermDebugNormal)
                return &it - lightPermutations;
            break;
        case kDebugPosition:
            if (it.permute == kLightPermDebugPosition)
                return &it - lightPermutations;
            break;
        case kDebugSSAO:
            if (it.permute == kLightPermDebugSSAO && !stencil)
                return &it - lightPermutations;
            break;
        }
    }
    int permute = 0;
    if (r_fog)
        permute |= kLightPermFog;
    if (r_ssao && !stencil)
        permute |= kLightPermSSAO;
    for (auto &it : lightPermutations)
        if (it.permute == permute)
            return &it - lightPermutations;
    return 0;
}

// Calculate the correct permutation to use for a given material
static void geomCalculatePermutation(material &mat, bool skeletal = false) {
    int permute = 0;
    if (skeletal)
        permute |= kGeomPermSkeletal;
    if (mat.diffuse)
        permute |= kGeomPermDiffuse;
    if (mat.normal)
        permute |= kGeomPermNormalMap;
    if (mat.spec && r_spec)
        permute |= kGeomPermSpecMap;
    if (mat.displacement && r_parallax)
        permute |= kGeomPermParallax;
    if (mat.specParams && r_spec)
        permute |= kGeomPermSpecParams;
    for (auto &it : geomPermutations) {
        if (it.permute == permute) {
            mat.permute = &it - geomPermutations;
            return;
        }
    }
}

///! Bounding box Rendering method
bool bboxMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/bbox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/bbox.fs"))
        return false;

    if (!finalize({ { 0, "position" } },
                  { { 0, "diffuseOut" } }))
    {
        return false;
    }

    m_WVPLocation = getUniformLocation("gWVP");
    m_colorLocation = getUniformLocation("gColor");

    return true;
}

void bboxMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void bboxMethod::setColor(const m::vec3 &color) {
    gl::Uniform3fv(m_colorLocation, 1, &color.x);
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

    if (!finalize({ { 0, "position"   },
                    { 1, "normal"     },
                    { 2, "texCoord"   },
                    { 3, "tangent"    },
                    { 4, "weights"    },
                    { 5, "bones"      } }
                , { { 0, "diffuseOut" },
                    { 1, "normalOut"  } }))
    {
        return false;
    }

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
    m_boneMatsLocation = getUniformLocation("gBoneMats");

    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, worldInverse.ptr());
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

void geomMethod::setBoneMats(size_t numJoints, const float *mats) {
    gl::UniformMatrix3x4fv(m_boneMatsLocation, numJoints, GL_FALSE, mats);
}

///! Composite Method
bool compositeMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    if (!addShader(GL_VERTEX_SHADER, "shaders/final.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/final.fs"))
        return false;
    if (!finalize({ { 0, "position" } }))
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_colorMapLocation = getUniformLocation("gColorMap");
    m_colorGradingMapLocation = getUniformLocation("gColorGradingMap");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    return true;
}

void compositeMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void compositeMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

void compositeMethod::setColorGradingTextureUnit(int unit) {
    gl::Uniform1i(m_colorGradingMapLocation, unit);
}

void compositeMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
    // TODO: frustum in final shader to do other things eventually
    //gl::Uniform2f(m_screenFrustumLocation, project.nearp, perspective.farp);
}

composite::composite()
    : m_fbo(0)
    , m_width(0)
    , m_height(0)
{
    memset(m_textures, 0, sizeof(m_textures));
}

composite::~composite() {
    destroy();
}

void composite::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_textures[0])
        gl::DeleteTextures(2, m_textures);
}

void composite::update(const m::perspective &p,
    const unsigned char *const colorGradingData)
{
    const size_t width = p.width;
    const size_t height = p.height;

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        gl::BindTexture(format, m_textures[kOutput]);
        gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
            GL_FLOAT, nullptr);
        gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Need to update color grading data if any is passed at all
    if (colorGradingData) {
        gl::BindTexture(GL_TEXTURE_3D, m_textures[kColorGrading]);
        gl::TexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 16, 16, 16, GL_RGB,
            GL_UNSIGNED_BYTE, colorGradingData);
    }
}

bool composite::init(const m::perspective &p, GLuint depth,
    const unsigned char *const colorGradingData)
{
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(2, m_textures);

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // output composite
    gl::BindTexture(format, m_textures[kOutput]);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT,
        nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format,
        m_textures[kOutput], 0);

    gl::BindTexture(format, depth);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        format, depth, 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    gl::DrawBuffers(1, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    // Color grading texture
    gl::BindTexture(GL_TEXTURE_3D, m_textures[kColorGrading]);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl::TexImage3D(GL_TEXTURE_3D, 0, GL_RGB8, 16, 16, 16, 0, GL_RGB,
        GL_UNSIGNED_BYTE, colorGradingData);

    return true;
}

void composite::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint composite::texture(size_t index) const {
    return m_textures[index];
}

///! renderer
world::world()
    : m_uploaded(false)
{
}

world::~world() {
    unload(false);
}

void world::unload(bool destroy) {
    for (auto &it : m_textures2D)
        delete it.second;
    for (auto &it : m_models)
        delete it.second;
    for (auto &it : m_billboards)
        delete it.second;

    if (destroy) {
        m_models.clear();
        m_billboards.clear();
        m_indices.destroy();
        m_vertices.destroy();
        m_textureBatches.destroy();
        m_textures2D.clear();
    }

    m_uploaded = false;
}

// HACK: Testing only
void testParticle(particle &p) {
    p.startAlpha = 0.6f;
    p.currentAlpha = 0.6f;
    p.color = m::vec3(1.0f, 1.0f, 1.0f);
    p.totalLifeTime = u::randf() * 0.2f;
    p.lifeTime = p.totalLifeTime;
    if (p.startOrigin == m::vec3::origin)
        p.startOrigin = m::vec3(0, 120, 0);
    else
        p.startOrigin += m::vec3::rand(0.05f, 0.05f, 0.05f);
    p.currentOrigin = p.startOrigin;
    p.startSize = 128.0f;
    p.currentSize = p.startSize;
    p.velocity = m::vec3(0.0f, 95.0f, 0.0f);
    p.respawn = true;
}

bool world::load(const kdMap &map) {
    // load skybox
    if (!m_skybox.load("textures/sky01"))
        return false;

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

    // HACK: Testing only
    if (!m_particles.load("textures/particle", &testParticle))
        neoFatal("failed to load particle");

    // HACK: Testing only
    if (!m_gun.load(m_textures2D, "models/lg"))
        neoFatal("failed to load gun");

    m_vertices = u::move(map.vertices);
    u::print("[world] => loaded\n");
    return true;
}

bool world::upload(const m::perspective &p, ::world *map) {
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
    if (!m_bbox.upload())
        neoFatal("failed to upload bbox");

    // HACK: Testing only
    if (!m_particles.upload())
        neoFatal("failed to upload particles");

    for (size_t i = 0; i < 128; i++) {
        particle p;
        testParticle(p);
        m_particles.addParticle(u::move(p));
    }

    // HACK: Testing only
    if (!m_gun.upload())
        neoFatal("failed to upload gun");

    // upload materials
    for (auto &it : m_textureBatches)
        if (!it.mat.upload())
            neoFatal("failed to upload world materials");

    geom::upload();

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(kdBinVertex), &m_vertices[0], GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(0));  // vertex
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(3));  // normals
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(6));  // texCoord
    gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), ATTRIB_OFFSET(8));  // tangent
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);
    gl::EnableVertexAttribArray(3);

    // upload data
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_STATIC_DRAW);

    // composite shader
    if (!m_compositeMethod.init())
        neoFatal("failed to initialize composite rendering method");
    m_compositeMethod.enable();
    m_compositeMethod.setColorTextureUnit(0);
    m_compositeMethod.setColorGradingTextureUnit(1);
    m_compositeMethod.setWVP(m_identity);

    // aa shader
    if (!m_aaMethod.init())
        neoFatal("failed to initialize aa rendering method");
    m_aaMethod.enable();
    m_aaMethod.setColorTextureUnit(0);
    m_aaMethod.setWVP(m_identity);

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

    // bbox method
    if (!m_bboxMethod.init())
        neoFatal("failed to initialize bounding box rendering method");
    m_bboxMethod.enable();
    m_bboxMethod.setColor({1.0f, 1.0f, 1.0f}); // White by default

    // setup g-buffer
    if (!m_gBuffer.init(p))
        neoFatal("failed to initialize world renderer");
    if (!m_ssao.init(p))
        neoFatal("failed to initialize world renderer");
    if (!m_final.init(p, m_gBuffer.texture(gBuffer::kDepth), map->getColorGrader().data()))
        neoFatal("failed to initialize world renderer");

    if (!m_ssaoMethod.init())
        neoFatal("failed to initialize ssao rendering method");

    // Setup default uniforms for ssao
    m_ssaoMethod.enable();
    m_ssaoMethod.setWVP(m_identity);
    m_ssaoMethod.setOccluderBias(0.05f);
    m_ssaoMethod.setSamplingRadius(15.0f);
    m_ssaoMethod.setAttenuation(1.5f, 0.0000005f);
    m_ssaoMethod.setNormalTextureUnit(ssaoMethod::kNormal);
    m_ssaoMethod.setDepthTextureUnit(ssaoMethod::kDepth);
    m_ssaoMethod.setRandomTextureUnit(ssaoMethod::kRandom);

    // setup occlusion stuff
    if (!m_queries.init())
        neoFatal("failed to initialize occlusion queries");

    u::print("[world] => uploaded\n");
    return m_uploaded = true;
}

void world::occlusionPass(const pipeline &pl, ::world *map) {
    if (!r_hoq)
        return;

    m_queries.update();
    for (auto &it : map->m_mapModels) {
        // Ignore if not loaded. The geometry pass will load it in later. We defer
        // to additional occlusion passes in that case.
        if (m_models.find(it->name) == m_models.end())
            continue;

        auto &mdl = m_models[it->name];

        pipeline p = pl;
        p.setWorld(it->position);
        p.setScale(it->scale + mdl->scale);

        const m::vec3 rot = mdl->rotate + it->rotate;
        m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
        m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
        m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
        m::mat4 rotate;
        (rz * ry * rx).getMatrix(&rotate);
        p.setRotate(rotate);

        pipeline bp;
        bp.setWorld(mdl->bounds().center());
        bp.setScale(mdl->bounds().size());
        const m::mat4 wvp = (p.projection() * p.view() * p.world()) * bp.world();

        // Get an occlusion query slot
        auto occlusionQuery = m_queries.add(wvp);
        if (!occlusionQuery)
            continue;

        it->occlusionQuery = *occlusionQuery;
    }

    // Dispatch all the queries
    m_queries.render();
}

NVAR(float, r_frame, "", 0.0f, 10000.0f, 1.0f);

void world::geometryPass(const pipeline &pl, ::world *map) {
    auto p = pl;

    // The scene pass will be writing into the gbuffer
    m_gBuffer.update(p.perspective());
    m_gBuffer.bindWriting();

    // Clear the depth and color buffers. This is a new scene pass.
    // We need depth testing as the scene pass will write into the depth
    // buffer. Blending isn't needed.
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    gl::Enable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);

    auto setup = [this](material &mat, pipeline &p, const m::mat4 &rw, bool skeletal = false) {
        // Calculate permutation incase a console variable changes
        geomCalculatePermutation(mat, skeletal);

        auto &permutation = geomPermutations[mat.permute];
        auto &method = m_geomMethods[mat.permute];
        method.enable();
        method.setWVP(p.projection() * p.view() * p.world());
        method.setWorld(rw);
        if (permutation.permute & kGeomPermParallax) {
            method.setEyeWorldPos(p.position());
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

        return &method;
    };

    // Render the map
    const m::mat4 &rw = p.world();
    gl::BindVertexArray(vao);
    for (auto &it : m_textureBatches) {
        setup(it.mat, p, rw);
        gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(GLuint) * it.start));
    }

    // Render map models
    for (auto &it : map->m_mapModels) {
        // Load map models on demand
        if (m_models.find(it->name) == m_models.end()) {
            u::unique_ptr<model> next(new model);
            if (!next->load(m_textures2D, it->name))
                neoFatal("failed to load model '%s'\n", it->name);
            if (!next->upload())
                neoFatal("failed to upload model '%s'\n", it->name);
            m_models[it->name] = next.release();
        } else {
            // Occlusion queries.
            //  TODO: Implement a space-partitioning data-structure to contain
            //  the bounding boxes of scene geometry in an efficient manner and
            //  apply HOQ's on those larger, more appropriate bounding volumes to
            //  disable / enable batches of models within those volumes.
            if (r_hoq && m_queries.passed(it->occlusionQuery))
                continue;

            auto &mdl = m_models[it->name];

            pipeline pm = p;
            pm.setWorld(it->position);
            pm.setScale(it->scale + mdl->scale);

            const m::vec3 rot = mdl->rotate + it->rotate;
            m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            pm.setRotate(rotate);

            if (mdl->animated()) {
                auto *method = setup(mdl->mat, pm, rw, true);
                method->setBoneMats(mdl->joints(), mdl->bones());
                // HACK: Testing only
                mdl->animate(it->curFrame);
                it->curFrame += 0.25f;
            } else {
                setup(mdl->mat, pm, rw);
            }
            mdl->render();
        }
    }

    // Only the scene pass needs to write to the depth buffer
    gl::Disable(GL_DEPTH_TEST);

    // Screen space ambient occlusion pass
    if (r_ssao) {
        // Read from the gbuffer, write to the ssao pass
        m_ssao.update(p.perspective());
        m_ssao.bindWriting();

        // Write to SSAO
        GLenum format = gl::has(gl::ARB_texture_rectangle)
            ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

        // Bind normal/depth/random
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kNormal);
        gl::BindTexture(format, m_gBuffer.texture(gBuffer::kNormal));
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kDepth);
        gl::BindTexture(format, m_gBuffer.texture(gBuffer::kDepth));
        gl::ActiveTexture(GL_TEXTURE0 + ssaoMethod::kRandom);
        gl::BindTexture(format, m_ssao.texture(ssao::kRandom));

        m_ssaoMethod.enable();
        m_ssaoMethod.setPerspective(p.perspective());
        m_ssaoMethod.setInverse((p.projection() * p.view()).inverse());

        m_quad.render();

        // TODO: X, Y + Blur
    }

    // Draw HUD elements to g-buffer
    if (r_debug != kDebugSSAO) {
        m_gBuffer.bindWriting();

        // Stencil test
        gl::Enable(GL_STENCIL_TEST);
        gl::Enable(GL_DEPTH_TEST);

        // Write 1s to stencil
        gl::StencilFunc(GL_ALWAYS, 1, 0xFF); // Stencil to 1
        gl::StencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
#if 1
        // HACK: Testing only
        {
            p.setRotation(m::quat());
            const m::vec3 rot = m::vec3(0, 180, 0);
            m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            p.setRotate(rotate);
            p.setScale({0.1, 0.1, 0.1});
            p.setPosition({-0.15, 0.2, -0.35});
            p.setWorld({0, 0, 0});
            setup(m_gun.mat, p, rw);
            m_gun.render();
        }
#endif

        gl::Disable(GL_STENCIL_TEST);
        gl::Disable(GL_DEPTH_TEST);
    }
}

static constexpr float kLightRadiusTweak = 1.11f;

void world::pointLightPass(const pipeline &pl, const ::world *const map) {
    pipeline p = pl;
    auto &method = m_pointLightMethod;
    method.enable();
    method.setPerspective(pl.perspective());
    method.setEyeWorldPos(pl.position());
    method.setInverse((p.projection() * p.view()).inverse());

    for (auto &it : map->m_pointLights) {
        float scale = it->radius * kLightRadiusTweak;

        // Frustum cull lights
        m_frustum.setup(it->position, p.rotation(), p.perspective());
        if (!m_frustum.testSphere(p.position(), scale))
            continue;

        p.setWorld(it->position);
        p.setScale({scale, scale, scale});

        const m::mat4 wvp = p.projection() * p.view() * p.world();
        method.setLight(*it);
        method.setWVP(wvp);

        const m::vec3 dist = it->position - p.position();
        scale += p.perspective().nearp + 1.0f;
        if (dist*dist >= scale*scale) {
            gl::DepthFunc(GL_LESS);
            gl::CullFace(GL_BACK);
        } else {
            gl::DepthFunc(GL_GEQUAL);
            gl::CullFace(GL_FRONT);
        }
        m_sphere.render();
    }
}

void world::spotLightPass(const pipeline &pl, const ::world *const map) {
    pipeline p = pl;
    auto &method = m_spotLightMethod;
    method.enable();
    method.setPerspective(pl.perspective());
    method.setEyeWorldPos(pl.position());
    method.setInverse((p.projection() * p.view()).inverse());

    for (auto &it : map->m_spotLights) {
        float scale = it->radius * kLightRadiusTweak;

        // Frustum cull lights
        m_frustum.setup(it->position, p.rotation(), p.perspective());
        if (!m_frustum.testSphere(p.position(), scale))
            continue;

        p.setWorld(it->position);
        p.setScale({scale, scale, scale});

        const m::mat4 wvp = p.projection() * p.view() * p.world();
        method.setLight(*it);
        method.setWVP(wvp);

        const m::vec3 dist = it->position - p.position();
        scale += p.perspective().nearp + 1.0f;
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
}

void world::lightingPass(const pipeline &pl, ::world *map) {
    auto p = pl;

    // Write to the final composite
    m_final.bindWriting();

    // Lighting will require blending
    gl::Enable(GL_BLEND);
    gl::BlendEquation(GL_FUNC_ADD);
    gl::BlendFunc(GL_ONE, GL_ONE);

    // Clear the final composite
    gl::Clear(GL_COLOR_BUFFER_BIT);

    // Need to read from the gbuffer and ssao buffer to do lighting
    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kColor);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kColor));
    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kNormal);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kNormal));
    gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kDepth);
    gl::BindTexture(format, m_gBuffer.texture(gBuffer::kDepth));

    if (!r_debug) {
        gl::Enable(GL_DEPTH_TEST);
        gl::DepthMask(GL_FALSE);
        gl::DepthFunc(GL_LESS);

        pointLightPass(pl, map);
        spotLightPass(pl, map);

        gl::Disable(GL_DEPTH_TEST);
    }

    // Change the blending function such that point and spot lights get fogged
    // as well.
    if (r_fog) {
        gl::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Two lighting passes for directional light (stencil and non stencil)
    gl::Enable(GL_STENCIL_TEST);
    gl::StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    for (size_t i = 0; i < 2; i++) {
        if ((r_ssao || r_debug == kDebugSSAO) && !i) {
            gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kOcclusion);
            gl::BindTexture(format, m_ssao.texture(ssao::kBuffer));
        }

        gl::StencilFunc(GL_EQUAL, !i, 0xFF);

        auto &method = m_directionalLightMethods[lightCalculatePermutation(!i)];
        method.enable();
        method.setLight(map->getDirectionalLight());
        method.setPerspective(pl.perspective());
        method.setEyeWorldPos(pl.position());
        method.setInverse((p.projection() * p.view()).inverse());
        if (r_fog)
            method.setFog(map->m_fog);
        m_quad.render();
    }

    gl::Disable(GL_STENCIL_TEST);
}

void world::forwardPass(const pipeline &pl, ::world *map) {
    m_final.bindWriting();

    gl::Enable(GL_DEPTH_TEST);

    // Skybox:
    //  Forwarded rendered and the last thing rendered to prevent overdraw.
    //
    //  Blending function ignores background since it contains fog contribution
    //  from directional lighting. The skybox is independently fogged with a
    //  gradient as to provide a coherent effect of world geometry fog reaching
    //  into the skybox.
    gl::BlendFunc(GL_ONE, GL_ZERO);
    m_skybox.render(pl, map->m_fog);

    // Editing aids
    static constexpr m::vec3 kHighlighted = { 1.0f, 0.0f, 0.0f };
    static constexpr m::vec3 kOutline = { 0.0f, 0.0f, 1.0f };

    if (varGet<int>("cl_edit").get()) {
        // World billboards:
        //  All billboards have pre-multiplied alpha to prevent strange blending
        //  issues around the edges.
        //
        //  Billboards are represented as one face so we have to disable
        //  culling too
        gl::Disable(GL_CULL_FACE);
        gl::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        for (auto &it : map->m_billboards) {
            // Load billboards on demand
            if (m_billboards.find(it.name) == m_billboards.end()) {
                u::unique_ptr<billboard> next(new billboard);
                if (!next->load(it.name))
                    neoFatal("failed to load billboard '%s'\n", it.name);
                if (!next->upload())
                    neoFatal("failed to upload billboard '%s'\n", it.name);
                m_billboards[it.name] = next.release();
            }

            if (it.bbox) {
                for (auto &jt : it.boards) {
                    if (!jt.highlight)
                        continue;

                    pipeline p = pl;
                    pipeline bp;
                    bp.setWorld(jt.position);
                    bp.setScale(it.size);
                    m_bboxMethod.enable();
                    m_bboxMethod.setColor(kOutline);
                    m_bboxMethod.setWVP((p.projection() * p.view() * p.world()) * bp.world());
                    m_bbox.render();
                }
            }

            auto &board = m_billboards[it.name];
            for (auto &jt : it.boards)
                board->add(jt.position);
            board->render(pl, it.size);
        }
        gl::Enable(GL_CULL_FACE);

        // Map models
        for (auto &it : map->m_mapModels) {
            auto &mdl = m_models[it->name];

            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale(it->scale + mdl->scale);

            const m::vec3 rot = mdl->rotate + it->rotate;
            m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            p.setRotate(rotate);

            pipeline bp;
            bp.setWorld(mdl->bounds().center());
            bp.setScale(mdl->bounds().size());
            m_bboxMethod.enable();
            m_bboxMethod.setColor(it->highlight ? kHighlighted : kOutline);
            m_bboxMethod.setWVP((p.projection() * p.view() * p.world()) * bp.world());
            m_bbox.render();
        }

        // Lights
        gl::Disable(GL_CULL_FACE);
        gl::PolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        m_bboxMethod.enable();
        m_bboxMethod.setColor(kHighlighted);
        for (auto &it : map->m_pointLights) {
            if (!it->highlight)
                continue;
            float scale = it->radius * kLightRadiusTweak;
            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale({scale, scale, scale});
            m_bboxMethod.setWVP(p.projection() * p.view() * p.world());
            m_sphere.render();
        }
        for (auto &it : map->m_spotLights) {
            if (!it->highlight)
                continue;
            float scale = it->radius * kLightRadiusTweak;
            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale({scale, scale, scale});
            m_bboxMethod.setWVP(p.projection() * p.view() * p.world());
            m_sphere.render();
        }

        gl::Enable(GL_CULL_FACE);
        gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Particles
    gl::Disable(GL_CULL_FACE);
    m_particles.update(pl);
    m_particles.render(pl);
    gl::Enable(GL_CULL_FACE);

    // Don't need depth testing or blending anymore
    gl::Disable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);

    // Render text showing what we're debugging
    const size_t x = neoWidth() / 2;
    const size_t y = neoHeight() - 20;
    switch (r_debug) {
        case kDebugDepth:
            gui::drawText(x, y, gui::kAlignCenter, "Depth", gui::RGBA(255, 0, 0));
            break;
        case kDebugNormal:
            gui::drawText(x, y, gui::kAlignCenter, "Normals", gui::RGBA(255, 0, 0));
            break;
        case kDebugPosition:
            gui::drawText(x, y, gui::kAlignCenter, "Position", gui::RGBA(255, 0, 0));
            break;
        case kDebugSSAO:
            gui::drawText(x, y, gui::kAlignCenter, "Ambient Occlusion", gui::RGBA(255, 0, 0));
            break;
        default:
            break;
    }
}

void world::compositePass(const pipeline &pl, ::world *map) {
    // We're going to be reading from the final composite
    auto &colorGrading = map->getColorGrader();
    if (colorGrading.updated()) {
        colorGrading.grade();
        m_final.update(pl.perspective(), map->getColorGrader().data());
        colorGrading.update();
    } else {
        m_final.update(pl.perspective(), nullptr);
    }

    if (r_fxaa) {
        m_final.bindWriting();
    } else {
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    gl::ActiveTexture(GL_TEXTURE0);
    gl::BindTexture(format, m_final.texture(composite::kOutput));

    gl::ActiveTexture(GL_TEXTURE1);
    gl::BindTexture(GL_TEXTURE_3D, m_final.texture(composite::kColorGrading));

    m_compositeMethod.enable();
    m_compositeMethod.setPerspective(pl.perspective());
    m_quad.render();

    if (r_fxaa) {
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(format, m_final.texture(composite::kOutput));
        m_aaMethod.enable();
        m_aaMethod.setPerspective(pl.perspective());
        m_quad.render();
    }
}

void world::render(const pipeline &pl, ::world *map) {
    occlusionPass(pl, map);
    geometryPass(pl, map);
    lightingPass(pl, map);
    forwardPass(pl, map);
    compositePass(pl, map);
}

}
