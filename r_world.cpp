#include <assert.h>
#include <math.h>

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
VAR(int, r_fog, "fog", 0, 1, 1);
VAR(int, r_smsize, "shadow map size", 16, 4096, 256);
VAR(int, r_smborder, "shadow map border", 0, 8, 3);
VAR(int, r_vignette, "vignette", 0, 1, 1);
VAR(float, r_vignette_radius, "vignette radius", 0.25f, 1.0f, 0.90f);
VAR(float, r_vignette_softness, "vignette softness", 0.0f, 1.0f, 0.45f);
VAR(float, r_vignette_opacity, "vignette opacity", 0.0f, 1.0f, 0.5f);
VAR(float, r_smbias, "shadow map bias", -10.0f, 10.0f, -0.1f);
VAR(float, r_smpolyfactor, "shadow map polygon offset factor", -1000.0f, 1000.0f, 1.0f);
VAR(float, r_smpolyoffset, "shadow map polygon offset units", -1000.0f, 1000.0f, 0.0f);
NVAR(int, r_debug, "debug visualizations", 0, 4, 0);
NVAR(int, r_stats, "rendering statistics", 0, 1, 0);
NVAR(int, r_reload, "reload shaders", 0, 1, 0);

namespace r {

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

static uint8_t calcTriangleSideMask(const m::vec3 &p1,
                                    const m::vec3 &p2,
                                    const m::vec3 &p3,
                                    float bias)
{
    // p1, p2, p3 are in the cubemap's local coordinate system
    // bias = border/(size - border)
    uint8_t mask = 0x3F;
    float dp1 = p1.x + p1.y, dn1 = p1.x - p1.y, ap1 = m::abs(dp1), an1 = m::abs(dn1),
          dp2 = p2.x + p2.y, dn2 = p2.x - p2.y, ap2 = m::abs(dp2), an2 = m::abs(dn2),
          dp3 = p3.x + p3.y, dn3 = p3.x - p3.y, ap3 = m::abs(dp3), an3 = m::abs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<4)
            | (dp1 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp2 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2))
            | (dp3 < 0 ? (1<<0)|(1<<2) : (2<<0)|(2<<2));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<4)
            | (dn1 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn2 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2))
            | (dn3 < 0 ? (1<<0)|(2<<2) : (2<<0)|(1<<2));

    dp1 = p1.y + p1.z, dn1 = p1.y - p1.z, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.y + p2.z, dn2 = p2.y - p2.z, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.y + p3.z, dn3 = p3.y - p3.z, ap3 = fabs(dp3), an3 = fabs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<0)
            | (dp1 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp2 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4))
            | (dp3 < 0 ? (1<<2)|(1<<4) : (2<<2)|(2<<4));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<0)
            | (dn1 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn2 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4))
            | (dn3 < 0 ? (1<<2)|(2<<4) : (2<<2)|(1<<4));

    dp1 = p1.z + p1.x, dn1 = p1.z - p1.x, ap1 = fabs(dp1), an1 = fabs(dn1),
    dp2 = p2.z + p2.x, dn2 = p2.z - p2.x, ap2 = fabs(dp2), an2 = fabs(dn2),
    dp3 = p3.z + p3.x, dn3 = p3.z - p3.x, ap3 = fabs(dp3), an3 = fabs(dn3);
    if (ap1 > bias*an1 && ap2 > bias*an2 && ap3 > bias*an3)
        mask &= (3<<2)
            | (dp1 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp2 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0))
            | (dp3 < 0 ? (1<<4)|(1<<0) : (2<<4)|(2<<0));
    if (an1 > bias*ap1 && an2 > bias*ap2 && an3 > bias*ap3)
        mask &= (3<<2)
            | (dn1 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn2 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0))
            | (dn3 < 0 ? (1<<4)|(2<<0) : (2<<4)|(1<<0));

    return mask;
}

///! lightChunk
world::lightChunk::lightChunk()
    : hash(0)
    , count(0)
    , visible(false)
    , ebo(0)
{
}

world::lightChunk::~lightChunk() {
    if (ebo)
        gl::DeleteBuffers(1, &ebo);
}

bool world::lightChunk::init() {
    gl::GenBuffers(1, &ebo);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    return true;
}

///! spotLightChunk
world::spotLightChunk::spotLightChunk()
    : light(nullptr)
{
}

world::spotLightChunk::spotLightChunk(spotLight *light)
    : light(light)
{
}

bool world::spotLightChunk::buildMesh(kdMap *map) {
    u::vector<size_t> triangleIndices;
    u::vector<GLuint> indices;
    map->inSphere(triangleIndices, light->position, light->radius);
    indices.reserve(triangleIndices.size() * 3 / 2);
    for (const auto &it : triangleIndices) {
        const auto &triangle = map->triangles[it];
        const m::vec3 p1 = map->vertices[triangle.v[0]].vertex - light->position;
        const m::vec3 p2 = map->vertices[triangle.v[1]].vertex - light->position;
        const m::vec3 p3 = map->vertices[triangle.v[2]].vertex - light->position;
        if (p1 * (p2 - p1).cross(p3 - p1) > 0)
            continue;
        for (const auto &it : triangle.v)
            indices.push_back(it);
    }
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof indices[0],
        &indices[0], GL_STATIC_DRAW);
    count = indices.size();
    return true;
}

///! pointLightChunk
world::pointLightChunk::pointLightChunk()
    : light(nullptr)
{
    memset(sideCounts, 0, sizeof sideCounts);
}

world::pointLightChunk::pointLightChunk(pointLight *light)
    : light(light)
{
    memset(sideCounts, 0, sizeof sideCounts);
}

bool world::pointLightChunk::buildMesh(kdMap *map) {
    u::vector<size_t> triangleIndices;
    u::vector<GLuint> indices[6];
    map->inSphere(triangleIndices, light->position, light->radius);
    for (size_t side = 0; side < 6; ++side)
        indices[side].reserve(triangleIndices.size() * 3 / 6);
    for (const auto &it : triangleIndices) {
        const auto &triangle = map->triangles[it];
        const m::vec3 p1 = map->vertices[triangle.v[0]].vertex - light->position;
        const m::vec3 p2 = map->vertices[triangle.v[1]].vertex - light->position;
        const m::vec3 p3 = map->vertices[triangle.v[2]].vertex - light->position;
        if (p1 * (p2 - p1).cross(p3 - p1) > 0)
            continue;
        const uint8_t mask = calcTriangleSideMask(p1, p2, p3, r_smborder / float(r_smsize - r_smborder));
        for (size_t side = 0; side < 6; ++side) {
            if (mask & (1 << side)) {
                for (const auto &it : triangle.v)
                    indices[side].push_back(it);
            }
        }
    }
    count = 0;
    for (size_t side = 0; side < 6; ++side) {
        sideCounts[side] = indices[side].size();
        count += sideCounts[side];
    }
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof indices[0][0], nullptr, GL_STATIC_DRAW);
    size_t offset = 0;
    for (size_t side = 0; side < 6; ++side) {
        if (sideCounts[side] > 0) {
            gl::BufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset * sizeof indices[0][0],
                sideCounts[side] * sizeof indices[0][0], &indices[side][0]);
        }
        offset += sideCounts[side];
    }
    return true;
}

/// NOTE: Testing only
struct dustSystem : particleSystem {
    dustSystem(const m::vec3 &ownerPosition);
protected:
    void initParticle(particle &p, const m::vec3 &ownerPosition);
    virtual float gravity() { return 98.0f; }
private:
    m::vec3 m_direction;
};

dustSystem::dustSystem(const m::vec3 &ownerPosition)
    : particleSystem("Dust System")
{
#if 0
    static constexpr size_t kParticles = 1024*2;
    m_particles.reserve(kParticles);
    for (size_t i = 0; i < kParticles-1; i++) {
        particle p;
        initParticle(p, ownerPosition);
        addParticle(u::move(p));
    }
#endif
}

void dustSystem::initParticle(particle &p, const m::vec3 &ownerPosition) {
    p.startAlpha = 1.0f;
    p.alpha = 1.0f;
    p.color = { 1.0f, 1.0f, 1.0f };
    p.totalLifeTime = 0.3f + u::randf() * 0.9f;
    p.lifeTime = p.totalLifeTime;
    p.origin = m::vec3::rand(0.05f, 0.0f, 0.05f) + ownerPosition;
    p.origin.y = ownerPosition.y - 3.0f;;
    p.size = 10.0f;
    p.startSize = 10.0f;
    p.velocity = (m_direction + m::vec3::rand(1.5f, 0.0f, 1.5f)).normalized() * 10.0f;
    p.velocity.y = 0.1f;
    p.respawn = true;
    p.visible = false;
}

///! world
static constexpr float kLightRadiusTweak = 1.11f;

world::world()
    : m_geomMethods(&geomMethods::instance())
    , m_map(nullptr)
    , m_kdWorld(nullptr)
    , m_uploaded(false)
{
}

world::~world() {
    unload(false);
}

bool world::load(kdMap *map) {
    // load skybox
    if (!m_skybox.load("textures/sky01"))
        return false;

    // make rendering batches for triangles which share the same texture
    for (size_t i = 0; i < map->textures.size(); i++) {
        renderTextureBatch batch;
        batch.start = m_indices.size();
        batch.index = i;
        for (size_t j = 0; j < map->triangles.size(); j++) {
            if (map->triangles[j].texture == i)
                for (size_t k = 0; k < 3; k++)
                    m_indices.push_back(map->triangles[j].v[k]);
        }
        batch.count = m_indices.size() - batch.start;
        m_textureBatches.push_back(batch);
    }

    m_kdWorld = map;

    // load materials
    for (auto &it : m_textureBatches) {
        if (!it.mat.load(m_textures2D, map->textures[it.index].name, "textures/"))
            neoFatal("failed to load material\n");
        it.mat.calculatePermutation();
    }

    // HACK: Testing only
    if (!m_gun.load(m_textures2D, "models/lg"))
        neoFatal("failed to load gun");

    u::print("[world] => loaded\n");
    return true;
}

bool world::upload(const m::perspective &p, ::world *map) {
    if (m_uploaded)
        return true;

    m_identity = m::mat4::identity();

    // hack
    m_particleSystems.push_back(new dustSystem(m::vec3::origin));

    // particles for entities
    for (auto *it : m_particleSystems)
        it->load("textures/particle"); // TODO: texture cache for particles

    // upload skybox
    if (!m_skybox.upload())
        neoFatal("failed to upload skybox");
    if (!m_quad.upload())
        neoFatal("failed to upload quad");
    if (!m_sphere.upload())
        neoFatal("failed to upload sphere");
    if (!m_bbox.upload())
        neoFatal("failed to upload bbox");
    if (!m_cone.upload())
        neoFatal("failed to upload cone");

    // HACK: Testing only
    if (!m_gun.upload())
        neoFatal("failed to upload gun");

    // upload particle systems
    for (auto *it : m_particleSystems)
        it->upload();

    // upload materials
    for (auto &it : m_textureBatches)
        if (!it.mat.upload())
            neoFatal("failed to upload world materials");

    m_map = map;

    geom::upload();

    const auto &vertices = m_kdWorld->vertices;
    m_memory = vertices.size() * sizeof(kdBinVertex);
    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_memory, &vertices[0], GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), u::offset_of(&kdBinVertex::vertex));  // vertex
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), u::offset_of(&kdBinVertex::normal));  // normals
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), u::offset_of(&kdBinVertex::coordinate));  // texCoord
    gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), u::offset_of(&kdBinVertex::tangent));  // tangent
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);
    gl::EnableVertexAttribArray(3);

    // upload data
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof m_indices[0], &m_indices[0], GL_STATIC_DRAW);
    m_memory += m_indices.size() * sizeof m_indices[0];

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

    if (!m_geomMethods->init())
        neoFatal("failed to initialize geometry rendering method");

    // directional light shader permutations
    static const size_t lightCount = sizeof lightPermutations / sizeof *lightPermutations;
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
    if (!m_pointLightMethods[0].init())
        neoFatal("failed to initialize point-light rendering method");
    m_pointLightMethods[0].enable();
    m_pointLightMethods[0].setColorTextureUnit(lightMethod::kColor);
    m_pointLightMethods[0].setNormalTextureUnit(lightMethod::kNormal);
    m_pointLightMethods[0].setDepthTextureUnit(lightMethod::kDepth);

    if (!m_pointLightMethods[1].init({"USE_SHADOWMAP"}))
        neoFatal("failed to initialize point-light rendering method");
    m_pointLightMethods[1].enable();
    m_pointLightMethods[1].setColorTextureUnit(lightMethod::kColor);
    m_pointLightMethods[1].setNormalTextureUnit(lightMethod::kNormal);
    m_pointLightMethods[1].setDepthTextureUnit(lightMethod::kDepth);
    m_pointLightMethods[1].setShadowMapTextureUnit(lightMethod::kShadowMap);

    // spot light method
    if (!m_spotLightMethods[0].init())
        neoFatal("failed to initialize spotlight rendering method");
    m_spotLightMethods[0].enable();
    m_spotLightMethods[0].setColorTextureUnit(lightMethod::kColor);
    m_spotLightMethods[0].setNormalTextureUnit(lightMethod::kNormal);
    m_spotLightMethods[0].setDepthTextureUnit(lightMethod::kDepth);

    if (!m_spotLightMethods[1].init({"USE_SHADOWMAP"}))
        neoFatal("failed to initialize spotlight rendering method");
    m_spotLightMethods[1].enable();
    m_spotLightMethods[1].setColorTextureUnit(lightMethod::kColor);
    m_spotLightMethods[1].setNormalTextureUnit(lightMethod::kNormal);
    m_spotLightMethods[1].setDepthTextureUnit(lightMethod::kDepth);
    m_spotLightMethods[1].setShadowMapTextureUnit(lightMethod::kShadowMap);

    // bbox method
    if (!m_bboxMethod.init())
        neoFatal("failed to initialize bounding box rendering method");
    m_bboxMethod.enable();
    m_bboxMethod.setColor({1.0f, 1.0f, 1.0f}); // White by default

    // default method
    if (!m_defaultMethod.init())
        neoFatal("failed to initialize default rendering method");
    m_defaultMethod.enable();
    m_defaultMethod.setColorTextureUnit(0);
    m_defaultMethod.setWVP(m_identity);

    // ssao method
    if (!m_ssaoMethod.init())
        neoFatal("failed to initialize ambient-occlusion rendering method");

    // Setup default uniforms for ssao
    m_ssaoMethod.enable();
    m_ssaoMethod.setWVP(m_identity);
    m_ssaoMethod.setOccluderBias(0.05f);
    m_ssaoMethod.setSamplingRadius(15.0f);
    m_ssaoMethod.setAttenuation(1.5f, 0.0000005f);
    m_ssaoMethod.setNormalTextureUnit(ssaoMethod::kNormal);
    m_ssaoMethod.setDepthTextureUnit(ssaoMethod::kDepth);
    m_ssaoMethod.setRandomTextureUnit(ssaoMethod::kRandom);

    // vignette method
    if (!m_vignetteMethod.init())
        neoFatal("failed to initialize vignette rendering method");

    // Setup default uniforms for vignette
    m_vignetteMethod.enable();
    m_vignetteMethod.setWVP(m_identity);
    m_vignetteMethod.setColorTextureUnit(0);

    // render buffers
    if (!m_gBuffer.init(p))
        neoFatal("failed to initialize world render buffer");
    if (!m_ssao.init(p))
        neoFatal("failed to initialize ambient-occlusion render buffer");
    if (!m_final.init(p, m_gBuffer.texture(gBuffer::kDepth)))
        neoFatal("failed to initialize final composite render buffer");
    if (!m_aa.init(p))
        neoFatal("failed to initialize anti-aliasing render buffer");
    if (!m_colorGrader.init(p, map->getColorGrader().data()))
        neoFatal("failed to initialize color grading render buffer");
    if (!m_vignette.init(p))
        neoFatal("failed to initialize vignette render buffer");

    if (!m_shadowMap.init(r_smsize*3, r_smsize*2))
        neoFatal("failed to initialize shadow map");
    if (!m_shadowMapMethod.init())
        neoFatal("failed to initialize shadow map method");
    m_shadowMapMethod.enable();
    m_shadowMapMethod.setWVP(m_identity);

    // Copy light entities into our rendering representation.
    for (auto &it : map->m_pointLights)
        m_culledPointLights.push_back({ it });
    for (auto &it : map->m_spotLights)
        m_culledSpotLights.push_back({ it });
    for (auto &it : m_culledPointLights)
        it.init();
    for (auto &it : m_culledSpotLights)
        it.init();

    u::print("[world] => uploaded\n");
    return m_uploaded = true;
}

void world::unload(bool destroy) {
    for (auto &it : m_textures2D)
        delete it.second;
    for (auto &it : m_models)
        delete it.second;
    for (auto &it : m_billboards)
        delete it.second;
    for (auto *it : m_particleSystems)
        delete it;

    if (destroy) {
        m_models.clear();
        m_billboards.clear();
        m_indices.destroy();
        m_textureBatches.destroy();
        m_textures2D.clear();
    }

    m_uploaded = false;
    u::print("[world] => unloaded\n");
}

void world::render(const pipeline &pl) {
    // TODO: rewrite world manager such that this hack is not needed
    if (m_map->m_needSync) {
        for (auto it = m_culledSpotLights.begin(), end = m_culledSpotLights.end(); it != end; ) {
            auto find = u::find(m_map->m_spotLights.begin(),
                                m_map->m_spotLights.end(),
                                it->light);
            if (find == m_map->m_spotLights.end()) {
                it = m_culledSpotLights.erase(it);
                end = m_culledSpotLights.end();
            } else {
                it++;
            }
        }
        for (auto it = m_culledPointLights.begin(), end = m_culledPointLights.end(); it != end; ) {
            auto find = u::find(m_map->m_pointLights.begin(),
                                m_map->m_pointLights.end(),
                                it->light);
            if (find == m_map->m_pointLights.end()) {
                it = m_culledPointLights.erase(it);
                end = m_culledPointLights.end();
            } else {
                it++;
            }
        }
        m_map->m_needSync = false;
    }

    // TODO: commands (u::function needs to be implemented)
    if (r_reload) {
        m_geomMethods->reload();
        for (auto &it : m_directionalLightMethods)
            it.reload();
        m_compositeMethod.reload();
        for (auto &it : m_pointLightMethods)
            it.reload();
        for (auto &it : m_spotLightMethods)
            it.reload();
        m_ssaoMethod.reload();
        m_bboxMethod.reload();
        m_aaMethod.reload();
        m_defaultMethod.reload();
        r_reload.set(0);
    }

    cullPass(pl);
    geometryPass(pl);
    lightingPass(pl);
    forwardPass(pl);
    compositePass(pl);
}

void world::cullPass(const pipeline &pl) {
    const float widthOffset = 0.5f * m_shadowMap.widthScale(r_smsize);
    const float heightOffset = 0.5f * m_shadowMap.heightScale(r_smsize);
    const float widthScale = 0.5f * m_shadowMap.widthScale(r_smsize - r_smborder);
    const float heightScale = 0.5f * m_shadowMap.heightScale(r_smsize - r_smborder);

    for (auto &it : m_culledSpotLights) {
        const auto &light = it.light;
        const float scale = light->radius * kLightRadiusTweak;
        m_frustum.setup(light->position, pl.rotation(), pl.perspective());
        it.visible = m_frustum.testSphere(pl.position(), scale);
        const auto hash = light->hash();
        if (it.visible && it.hash != hash) {
            it.buildMesh(m_kdWorld);
            it.hash = hash;
            if (light->castShadows) {
                it.transform = m::mat4::translate({widthOffset, heightOffset, 0.5f}) *
                               m::mat4::scale({widthScale, heightScale, 0.5f}) *
                               m::mat4::project(light->cutOff, 1.0f / light->radius, sqrtf(3.0f), r_smbias / light->radius) *
                               m::mat4::lookat(light->direction, m::vec3::yAxis) *
                               m::mat4::scale(1.0f / light->radius) *
                               m::mat4::translate(-light->position);
            }
        }
    }

    for (auto &it : m_culledPointLights) {
        const auto &light = it.light;
        const float scale = light->radius * kLightRadiusTweak;
        m_frustum.setup(light->position, pl.rotation(), pl.perspective());
        it.visible = m_frustum.testSphere(pl.position(), scale);
        const auto hash = light->hash();
        if (it.visible && it.hash != hash) {
            it.buildMesh(m_kdWorld);
            it.hash = hash;
            if (light->castShadows) {
                it.transform = m::mat4::translate({widthOffset, heightOffset, 0.5f}) *
                               m::mat4::scale({widthScale, heightScale, 0.5f}) *
                               m::mat4::project(90.0f, 1.0f / light->radius, sqrtf(3.0f), r_smbias / light->radius) *
                               m::mat4::scale(1.0f / light->radius);
            }
        }
    }
}

void world::geometryPass(const pipeline &pl) {
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

    // Render the map
    const m::mat4 &rw = p.world();
    gl::BindVertexArray(vao);
    for (auto &it : m_textureBatches) {
        it.mat.bind(p, rw);
        gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(it.start * sizeof m_indices[0]));
    }

    // Render map models
    for (auto &it : m_map->m_mapModels) {
        // Load map models on demand
        if (m_models.find(it->name) == m_models.end()) {
            u::unique_ptr<model> next(new model);
            if (!next->load(m_textures2D, it->name))
                neoFatal("failed to load model '%s'\n", it->name);
            if (!next->upload())
                neoFatal("failed to upload model '%s'\n", it->name);
            m_models[it->name] = next.release();
        } else {
            auto &mdl = m_models[it->name];

            pipeline pm = p;
            pm.setWorld(it->position);
            pm.setScale(it->scale + mdl->scale);

            const m::vec3 rot = mdl->rotate + it->rotate;
            const m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            const m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            const m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            pm.setRotate(rotate);

            if (mdl->animated()) {
                // HACK: Testing only
                mdl->animate(it->curFrame);
                it->curFrame += 0.25f;
            }
            mdl->render(pm, rw);
        }
    }

    // Only the scene pass needs to write to the depth buffer
    gl::Disable(GL_DEPTH_TEST);

    // Screen space ambient occlusion pass
    if (r_ssao) {
        // Read from the gbuffer, write to the ssao pass
        m_ssao.update(pl.perspective());
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
        // NOTE: Testing Only
        {
            p.setRotation(m::quat());
            const m::vec3 rot = m::vec3(0, 180, 0);
            const m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            const m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            const m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            p.setRotate(rotate);
            p.setScale({0.1, 0.1, 0.1});
            p.setPosition({-0.15, 0.2, -0.35});
            p.setWorld({0, 0, 0});
            m_gun.render(p, rw);
        }
#endif

        gl::Disable(GL_STENCIL_TEST);
        gl::Disable(GL_DEPTH_TEST);
    }
}

void world::lightingPass(const pipeline &pl) {
    auto p = pl;

    // Write to the final composite
    m_final.update(p.perspective());
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

        pointLightPass(pl);
        spotLightPass(pl);

        gl::Disable(GL_DEPTH_TEST);
    }

    // Change the blending function such that point and spot lights get fogged
    // as well.
    if (r_fog)
        gl::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // Two lighting passes for directional light (stencil and non stencil)
    gl::Enable(GL_STENCIL_TEST);
    gl::StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    directionalLightPass(pl, true);
    directionalLightPass(pl, false);

    gl::Disable(GL_STENCIL_TEST);
}

void world::forwardPass(const pipeline &pl) {
    gl::Enable(GL_DEPTH_TEST);

    // Skybox:
    //  Forwarded rendered and the last thing rendered to prevent overdraw.
    m_skybox.render(pl, m_map->m_fog);

    // Editing aids
    static constexpr m::vec3 kHighlighted = { 1.0f, 0.0f, 0.0f };
    static constexpr m::vec3 kOutline = { 0.0f, 0.0f, 1.0f };

    gl::DepthMask(GL_FALSE);
    if (varGet<int>("cl_edit").get()) {
        // World billboards:
        //  All billboards have pre-multiplied alpha to prevent strange blending
        //  issues around the edges.
        //
        //  Billboards are represented as one face so we have to disable
        //  culling too
        gl::Disable(GL_CULL_FACE);
        gl::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        for (auto &it : m_map->m_billboards) {
            // Load billboards on demand
            if (m_billboards.find(it.name) == m_billboards.end()) {
                u::unique_ptr<billboard> next(new billboard(it.description));
                if (!next->load(it.name))
                    neoFatal("failed to load billboard '%s'\n", it.name);
                if (!next->upload())
                    neoFatal("failed to upload billboard '%s'\n", it.name);
                m_billboards[it.name] = next.release();
            }

            if (it.bbox) {
                for (const auto &jt : it.boards) {
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

            const auto &board = m_billboards[it.name];
            for (const auto &jt : it.boards)
                board->add(jt.position, billboard::kSide, m::vec3::origin, m::vec3::yAxis);
            board->render(pl, it.size);
        }
        gl::Enable(GL_CULL_FACE);

        // Map models
        for (auto &it : m_map->m_mapModels) {
            auto &mdl = m_models[it->name];

            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale(it->scale + mdl->scale);

            const m::vec3 rot = mdl->rotate + it->rotate;
            const m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            const m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            const m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
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
        for (const auto &it : m_map->m_pointLights) {
            if (!it->highlight)
                continue;
            const float scale = it->radius * kLightRadiusTweak;
            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale({scale, scale, scale});
            m_bboxMethod.setWVP(p.projection() * p.view() * p.world());
            m_sphere.render();
        }

        for (const auto &it : m_map->m_spotLights) {
            if (!it->highlight)
                continue;
            const float scale = it->radius * kLightRadiusTweak;
            pipeline p = pl;
            p.setWorld(it->position);
            p.setScale({it->cutOff, scale, it->cutOff});

            const m::vec3 rot = it->direction;
            const m::quat rx(m::toRadian(rot.x), m::vec3::xAxis);
            const m::quat ry(m::toRadian(rot.y), m::vec3::yAxis);
            const m::quat rz(m::toRadian(rot.z), m::vec3::zAxis);
            m::mat4 rotate;
            (rz * ry * rx).getMatrix(&rotate);
            p.setRotate(rotate);

            m_bboxMethod.setWVP(p.projection() * p.view() * p.world());
            m_cone.render(false);
        }

        gl::Enable(GL_CULL_FACE);
        gl::PolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    // Particles
    for (auto *it : m_particleSystems) {
        it->update(pl);

        // Bind the depth buffer for soft particles
        gl::ActiveTexture(GL_TEXTURE1);
        gl::BindTexture(GL_TEXTURE_RECTANGLE, m_gBuffer.texture(gBuffer::kDepth));
        it->render(pl);
    }
    // Don't need depth testing or blending anymore
    gl::DepthMask(GL_TRUE);
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

    // Render some memory statistics
    if (r_stats) {
        size_t y = neoHeight() / 2 + 20 * 2;
        // Particle memory
        gui::drawText(10, y, gui::kAlignLeft, "WORLD", gui::RGBA(255, 255, 255));
        y -= 20;
        const auto format = u::format("Geometry: %s", u::sizeMetric(m_memory));
        gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
        y -= 20;
        if (m_particleSystems.size()) {
            gui::drawText(10, y, gui::kAlignLeft, "PARTICLE", gui::RGBA(255, 255, 255));
            y -= 20;
            for (auto *it : m_particleSystems) {
                if (it->memory() == 0)
                    continue;
                const auto format = u::format("%s: %s", it->description(), u::sizeMetric(it->memory()));
                gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
                y -= 20;
            }
        }
        // Billboard memory
        if (m_billboards.size()) {
            gui::drawText(10, y, gui::kAlignLeft, "BILLBOARD", gui::RGBA(255, 255, 255));
            y -= 20;
            for (const auto &it : m_billboards) {
                const billboard *const b = it.second;
                if (b->memory() == 0)
                    continue;
                const auto format = u::format("%s: %s", b->description(), u::sizeMetric(b->memory()));
                gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
                y -= 20;
            }
        }
        // Shadow EBO memory
        size_t slShadowEBO = 0,
               plShadowEBO = 0;
        for (auto &it : m_culledSpotLights)
            slShadowEBO += it.count * sizeof(GLuint);
        for (auto &it : m_culledPointLights)
            plShadowEBO += it.count * sizeof(GLuint);
        if (slShadowEBO || plShadowEBO) {
            gui::drawText(10, y, gui::kAlignLeft, "SHADOW EBO", gui::RGBA(255, 255, 255));
            y -= 20;
            if (slShadowEBO) {
                const auto format = u::format("Spot Lights: %s", u::sizeMetric(slShadowEBO));
                gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
                y -= 20;
            }
            if (plShadowEBO) {
                const auto format = u::format("Point Lights: %s", u::sizeMetric(plShadowEBO));
                gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
                y -= 20;
            }
        }
        // Texture memory
        size_t textureMemory = 0;
        for (const auto &it : m_textures2D) {
            const texture2D *const tex = it.second;
            textureMemory += tex->memory();
        }
        if (textureMemory) {
            gui::drawText(10, y, gui::kAlignLeft, "TEXTURE", gui::RGBA(255, 255, 255));
            y -= 20;
            const auto format = u::format("2D Assets: %s", u::sizeMetric(textureMemory));
            gui::drawText(20, y, gui::kAlignLeft, format.c_str(), gui::RGBA(255, 255, 255));
            y -= 20;
        }
    }
}

void world::compositePass(const pipeline &pl) {
    auto &colorGrading = m_map->getColorGrader();
    if (colorGrading.updated()) {
        colorGrading.grade();
        m_colorGrader.update(pl.perspective(), colorGrading.data());
        colorGrading.update();
    } else {
        m_colorGrader.update(pl.perspective(), nullptr);
    }

    // Writing to color grader
    m_colorGrader.bindWriting();

    const GLenum format = gl::has(gl::ARB_texture_rectangle) ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // take final composite on unit 0
    gl::ActiveTexture(GL_TEXTURE0);
    gl::BindTexture(format, m_final.texture());

    // take color grading lut on unit 1
    gl::ActiveTexture(GL_TEXTURE1);
    gl::BindTexture(GL_TEXTURE_3D, m_colorGrader.texture(grader::kColorGrading));

    // render to color grading buffer
    m_compositeMethod.enable();
    m_compositeMethod.setPerspective(pl.perspective());
    m_quad.render();

    // apply vignette now
    if (r_vignette) {
        m_vignette.bindWriting();

        // take color grading result on unit 0
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(format, m_colorGrader.texture(grader::kOutput));

        // render vignette
        m_vignetteMethod.enable();
        m_vignetteMethod.setPerspective(pl.perspective());
        m_vignetteMethod.setProperties(r_vignette_radius,
                                       r_vignette_softness,
                                       r_vignette_opacity);
        m_quad.render();
    }

    if (r_fxaa) {
        // write to aa buffer
        m_aa.bindWriting();

        gl::ActiveTexture(GL_TEXTURE0);
        if (r_vignette) {
            // take vignette result on unit 0
            gl::BindTexture(format, m_vignette.texture());
        } else {
            // take color grading result on unit 0
            gl::BindTexture(format, m_colorGrader.texture(grader::kOutput));
        }

        // render aa buffer
        m_aaMethod.enable();
        m_aaMethod.setPerspective(pl.perspective());
        m_quad.render();

        // write to window buffer
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        // take the aa result on unit 0
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(format, m_aa.texture());

        // render window buffer
        m_defaultMethod.enable();
        m_defaultMethod.setPerspective(pl.perspective());
        m_quad.render();

    } else {
        // write to window buffer
        gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        gl::ActiveTexture(GL_TEXTURE0);
        if (r_vignette) {
            // take vignette result on unit 0
            gl::BindTexture(format, m_vignette.texture());
        } else {
            // take color grading result on unit 0
            gl::BindTexture(format, m_colorGrader.texture(grader::kOutput));
        }

        // render window buffer
        m_defaultMethod.enable();
        m_defaultMethod.setPerspective(pl.perspective());
        m_quad.render();
    }
}

void world::pointLightPass(const pipeline &pl) {
    pipeline p = pl;

    gl::DepthMask(GL_FALSE);

    for (auto &plc : m_culledPointLights) {
        if (!plc.visible)
            continue;
        auto &it = plc.light;
        float scale = it->radius * kLightRadiusTweak;

        pointLightMethod *method = &m_pointLightMethods[0];

        // Only bother if these are casting shadows
        if (it->castShadows) {
            pointLightShadowPass(&plc);

            gl::DepthMask(GL_FALSE);

            method = &m_pointLightMethods[1];

            gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kShadowMap);
            gl::BindTexture(GL_TEXTURE_2D, m_shadowMap.texture());

            method->enable();
            method->setLightWVP(plc.transform);
        } else {
            method->enable();
        }

        method->setPerspective(pl.perspective());
        method->setEyeWorldPos(pl.position());
        method->setInverse((p.projection() * p.view()).inverse());

        p.setWorld(it->position);
        p.setScale({scale, scale, scale});

        const m::mat4 wvp = p.projection() * p.view() * p.world();
        method->setLight(*it);
        method->setWVP(wvp);

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

void world::pointLightShadowPass(const pointLightChunk *const plc) {
    const pointLight *const pl = plc->light;
    gl::DepthMask(GL_TRUE);
    gl::DepthFunc(GL_LEQUAL);

    m_shadowMap.update(r_smsize*3, r_smsize*2);
    m_shadowMap.bindWriting();
    gl::Clear(GL_DEPTH_BUFFER_BIT);

    if (r_smpolyfactor || r_smpolyoffset) {
        gl::PolygonOffset(r_smpolyfactor, r_smpolyoffset);
        gl::Enable(GL_POLYGON_OFFSET_FILL);
    }

    gl::Enable(GL_SCISSOR_TEST);

    m_shadowMapMethod.enable();

    // v = identity
    // for i=0 to 6:
    //  if i < 2: swap(v.a, v.c)
    //  else if i < 4: swap(v.b, v.c)
    //  if even(i): v.c = -v.c
    //  c = (odd(i) ^ (i >= 4)) ? GL_FRONT : GL_BACK
    static const struct {
        m::mat4 view;
        GLenum cullFace;
    } kSideViews[] = {
        { { {  0.0f,  0.0f,  1.0f,  0.0f }, {  0.0f,  1.0f,  0.0f,  0.0f },
            { -1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_BACK },
        { { {  0.0f,  0.0f,  1.0f,  0.0f }, {  0.0f,  1.0f,  0.0f,  0.0f },
            {  1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_FRONT },
        { { {  1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f,  0.0f },
            {  0.0f, -1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_BACK },
        { { {  1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  1.0f,  0.0f },
            {  0.0f,  1.0f,  0.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_FRONT },
        { { {  1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f,  0.0f },
            {  0.0f,  0.0f, -1.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_FRONT },
        { { {  1.0f,  0.0f,  0.0f,  0.0f }, {  0.0f,  1.0f,  0.0f,  0.0f },
            {  0.0f,  0.0f,  1.0f,  0.0f }, {  0.0f,  0.0f,  0.0f,  1.0f } }, GL_BACK }
    };

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, plc->ebo);
    const float borderScale = float(r_smsize - r_smborder) / r_smsize;
    size_t offset = 0;
    for (size_t side = 0; side < 6; ++side) {
        if (plc->sideCounts[side] <= 0)
            continue;

        const auto &view = kSideViews[side];
        m_shadowMapMethod.setWVP(m::mat4::scale({borderScale, borderScale, 1.0f}) *
                                 m::mat4::project(90.0f, 1.0f / pl->radius, sqrtf(3.0f)) *
                                 view.view *
                                 m::mat4::scale(1.0f /  pl->radius) *
                                 m::mat4::translate(-pl->position));

        const size_t x = r_smsize * (side / 2);
        const size_t y = r_smsize * (side % 2);
        gl::Viewport(x, y, r_smsize, r_smsize);
        gl::Scissor(x, y, r_smsize, r_smsize);
        gl::CullFace(view.cullFace);
        gl::DrawElements(GL_TRIANGLES, plc->sideCounts[side], GL_UNSIGNED_INT,
            (const GLvoid *)(sizeof(GLuint)*offset));

        offset += plc->sideCounts[side];
    }
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    gl::Viewport(0, 0, neoWidth(), neoHeight());

    gl::Disable(GL_SCISSOR_TEST);

    if (r_smpolyfactor || r_smpolyoffset)
        gl::Disable(GL_POLYGON_OFFSET_FILL);

    m_final.bindWriting();
}

void world::spotLightPass(const pipeline &pl) {
    pipeline p = pl;

    gl::DepthMask(GL_FALSE);

    for (auto &slc : m_culledSpotLights) {
        if (!slc.visible)
            continue;
        auto &sl = slc.light;
        float scale = sl->radius * kLightRadiusTweak;

        spotLightMethod *method = &m_spotLightMethods[0];

        // Only bother if these are casting shadows
        if (sl->castShadows) {
            spotLightShadowPass(&slc);

            gl::DepthMask(GL_FALSE);

            method = &m_spotLightMethods[1];

            gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kShadowMap);
            gl::BindTexture(GL_TEXTURE_2D, m_shadowMap.texture());

            method->enable();
            method->setLightWVP(slc.transform);
        } else {
            method->enable();
        }

        method->setPerspective(pl.perspective());
        method->setEyeWorldPos(pl.position());
        method->setInverse((p.projection() * p.view()).inverse());

        p.setWorld(sl->position);
        p.setScale({scale, scale, scale});

        const m::mat4 wvp = p.projection() * p.view() * p.world();
        method->setLight(*sl);
        method->setWVP(wvp);

        const m::vec3 dist = sl->position - p.position();
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

void world::spotLightShadowPass(const spotLightChunk *const slc) {
    const spotLight *const sl = slc->light;
    gl::DepthMask(GL_TRUE);
    gl::DepthFunc(GL_LEQUAL);
    gl::CullFace(GL_BACK);

    if (r_smpolyfactor || r_smpolyoffset) {
        gl::PolygonOffset(r_smpolyfactor, r_smpolyoffset);
        gl::Enable(GL_POLYGON_OFFSET_FILL);
    }

    gl::Enable(GL_SCISSOR_TEST);
    gl::Scissor(0, 0, r_smsize, r_smsize);

    m_shadowMap.update(r_smsize*3, r_smsize*2);
    m_shadowMap.bindWriting();
    gl::Clear(GL_DEPTH_BUFFER_BIT);

    const float borderScale = float(r_smsize - r_smborder) / r_smsize;
    m_shadowMapMethod.enable();
    m_shadowMapMethod.setWVP(m::mat4::scale({borderScale, borderScale, 1.0f}) *
                             m::mat4::project(sl->cutOff, 1.0f / sl->radius, sqrtf(3.0f)) *
                             m::mat4::lookat(sl->direction, m::vec3::yAxis) *
                             m::mat4::scale(1.0f / sl->radius) *
                             m::mat4::translate(-sl->position));

    // Draw the scene from the lights perspective into the shadow map
    gl::Viewport(0, 0, r_smsize, r_smsize);
    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, slc->ebo);
    gl::DrawElements(GL_TRIANGLES, slc->count, GL_UNSIGNED_INT, 0);
    gl::Viewport(0, 0, neoWidth(), neoHeight());
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

    gl::Disable(GL_SCISSOR_TEST);

    if (r_smpolyfactor || r_smpolyoffset)
        gl::Disable(GL_POLYGON_OFFSET_FILL);

    m_final.bindWriting();
}

void world::directionalLightPass(const pipeline &pl, bool stencil) {
    auto p = pl;

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    if ((r_ssao || r_debug == kDebugSSAO) && stencil) {
        gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kOcclusion);
        gl::BindTexture(format, m_ssao.texture(ssao::kBuffer));
    }

    gl::StencilFunc(GL_EQUAL, stencil, 0xFF);

    auto &method = m_directionalLightMethods[lightCalculatePermutation(stencil)];
    method.enable();
    method.setLight(m_map->getDirectionalLight());
    method.setPerspective(pl.perspective());
    method.setEyeWorldPos(pl.position());
    method.setInverse((p.projection() * p.view()).inverse());
    if (r_fog)
        method.setFog(m_map->m_fog);
    m_quad.render();
}

}
