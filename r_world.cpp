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
VAR(int, r_smsize, "shadow map size", 128, 4096, 1024);
NVAR(int, r_debug, "debug visualizations", 0, 4, 0);

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

///! Bounding box Rendering method
bool bboxMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/bbox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/bbox.fs"))
        return false;
    if (!finalize({ "position" }, { "diffuseOut" }))
        return false;

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
    if (!finalize({ "position" }))
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
    , m_texture(0)
    , m_width(0)
    , m_height(0)
{
}

composite::~composite() {
    destroy();
}

void composite::destroy() {
    if (m_fbo)
        gl::DeleteFramebuffers(1, &m_fbo);
    if (m_texture)
        gl::DeleteTextures(1, &m_texture);
}

void composite::update(const m::perspective &p) {
    const size_t width = p.width;
    const size_t height = p.height;

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;
    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA,
        GL_FLOAT, nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool composite::init(const m::perspective &p, GLuint depth) {
    m_width = p.width;
    m_height = p.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(1, &m_texture);

    GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

    // output composite
    gl::BindTexture(format, m_texture);
    gl::TexImage2D(format, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_FLOAT,
        nullptr);
    gl::TexParameteri(format, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(format, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format,
        m_texture, 0);

    gl::BindTexture(format, depth);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        format, depth, 0);

    static GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    gl::DrawBuffers(1, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void composite::bindWriting() {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
}

GLuint composite::texture() const {
    return m_texture;
}

///! renderer
world::world()
    : m_geomMethods(&geomMethods::instance())
    , m_uploaded(false)
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
    for (auto *it : m_particleSystems)
        delete it;

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

// a particle!
struct dustSystem : particleSystem {
    dustSystem(const m::vec3 &ownerPosition);
protected:
    void initParticle(particle &p, const m::vec3 &ownerPosition);
    virtual float getGravity() { return 98.0f; }
private:
    m::vec3 m_direction;
};

dustSystem::dustSystem(const m::vec3 &ownerPosition) {
    static constexpr size_t kParticles = 2048*2;
    m_particles.reserve(kParticles);
    for (size_t i = 0; i < kParticles; i++) {
        particle p;
        initParticle(p, ownerPosition);
        addParticle(u::move(p));
    }
    m_direction = { 0.0f, 0.0f, -1.0f };
    if (m_direction*m_direction > 0.1f)
        m_direction = m_direction.normalized() * 12.0f;
}

void dustSystem::initParticle(particle &p, const m::vec3 &ownerPosition) {
    p.startAlpha = 1.0f;
    p.alpha = 1.0f;
    p.color = { 1.0f, 1.0f, 1.0f };
    p.totalLifeTime = 0.3f + u::randf() * 0.9f;
    p.lifeTime = p.totalLifeTime;
    p.origin = m::vec3::rand(0.05f, 0.0f, 0.05f) + ownerPosition;
    p.origin.y = ownerPosition.y - 3.0f;;
    p.size = 1.0f;
    p.startSize = 1.0f;
    p.velocity = (m_direction + m::vec3::rand(1.5f, 0.0f, 1.5f)).normalized() * 10.0f;
    p.velocity.y = 0.1f;
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
        it.mat.calculatePermutation();
    }

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

    if (!m_geomMethods->init())
        neoFatal("failed to initialize geometry rendering method");

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

    if (!m_spotLightShadowMap.init(r_smsize))
        neoFatal("failed to initialize shadow map");
    if (!m_shadowMapMethod.init())
        neoFatal("failed to initialize shadow map method");
    m_shadowMapMethod.enable();
    m_shadowMapMethod.setWVP(m_identity);

    // setup occlusion stuff
    if (!m_queries.init())
        neoFatal("failed to initialize occlusion queries");

    u::print("[world] => uploaded\n");
    return m_uploaded = true;
}

static constexpr float kLightRadiusTweak = 1.11f;

void world::cullPass(const pipeline &pl, ::world *map) {
    m_culledSpotLights.clear();
    for (auto &it : map->m_spotLights) {
        const float scale = it->radius * kLightRadiusTweak;
        m_frustum.setup(it->position, pl.rotation(), pl.perspective());
        if (m_frustum.testSphere(pl.position(), scale))
            m_culledSpotLights.push_back(it);
    }
    m_culledPointLights.clear();
    for (auto &it : map->m_pointLights) {
        const float scale = it->radius * kLightRadiusTweak;
        m_frustum.setup(it->position, pl.rotation(), pl.perspective());
        if (m_frustum.testSphere(pl.position(), scale))
            m_culledPointLights.push_back(it);
    }
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

    // Render the map
    const m::mat4 &rw = p.world();
    gl::BindVertexArray(vao);
    for (auto &it : m_textureBatches) {
        it.mat.bind(p, rw);
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
            m_gun.render(p, rw);
        }
#endif

        gl::Disable(GL_STENCIL_TEST);
        gl::Disable(GL_DEPTH_TEST);
    }
}

void world::pointLightPass(const pipeline &pl) {
    pipeline p = pl;
    auto &method = m_pointLightMethod;
    method.enable();
    method.setPerspective(pl.perspective());
    method.setEyeWorldPos(pl.position());
    method.setInverse((p.projection() * p.view()).inverse());

    for (auto &it : m_culledPointLights) {
        float scale = it->radius * kLightRadiusTweak;

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

void world::spotLightPass(const pipeline &pl) {
    pipeline p = pl;

    for (auto &sl : m_culledSpotLights) {
        float scale = sl->radius * kLightRadiusTweak;

        spotLightMethod &method = m_spotLightMethods[0];

        // Only bother if these are casting shadows
        if (sl->castShadows) {
            spotLightShadowPass(sl);

            method = m_spotLightMethods[1];

            gl::ActiveTexture(GL_TEXTURE0 + lightMethod::kShadowMap);
            gl::BindTexture(GL_TEXTURE_2D, m_spotLightShadowMap.texture());

            // Calculate lighting matrix
            const float kShadowBias = -0.00005f;
            m::mat4 translate, rotate, project, projectScale, projectTranslate;
            translate.setTranslateTrans(-sl->position.x, -sl->position.y, -sl->position.z);
            rotate.setCameraTrans(sl->direction, m::vec3::yAxis);
            project.setSpotLightPerspectiveTrans(sl->cutOff, sl->radius);

            // Calculate shadow map scale
            projectScale.setScaleTrans(0.5f, 0.5f, 0.5f);
            projectTranslate.setTranslateTrans(0.5f, 0.5f, 0.5f + kShadowBias);

            method.enable();
            method.setLightWVP(projectTranslate * projectScale * project * rotate * translate);
        } else {
            method.enable();
        }

        method.setPerspective(pl.perspective());
        method.setEyeWorldPos(pl.position());
        method.setInverse((p.projection() * p.view()).inverse());

        p.setWorld(sl->position);
        p.setScale({scale, scale, scale});

        const m::mat4 wvp = p.projection() * p.view() * p.world();
        method.setLight(*sl);
        method.setWVP(wvp);

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

void world::lightingPass(const pipeline &pl, ::world *map) {
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
        gl::DepthMask(GL_FALSE);
        gl::DepthFunc(GL_LESS);

        pointLightPass(pl);
        spotLightPass(pl);

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
    for (auto *it : m_particleSystems) {
        it->update(pl);
        it->render(pl);
    }
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
    auto &colorGrading = map->getColorGrader();
    if (colorGrading.updated()) {
        colorGrading.grade();
        m_colorGrader.update(pl.perspective(), colorGrading.data());
        colorGrading.update();
    } else {
        m_colorGrader.update(pl.perspective(), nullptr);
    }

    // Writing to color grader
    m_colorGrader.bindWriting();

    const GLenum format = gl::has(gl::ARB_texture_rectangle)
        ? GL_TEXTURE_RECTANGLE : GL_TEXTURE_2D;

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

    if (r_fxaa) {
        // write to aa buffer
        m_aa.bindWriting();

        // take color grading result on unit 0
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(format, m_colorGrader.texture(grader::kOutput));

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

        // take color grading result on unit 0
        gl::ActiveTexture(GL_TEXTURE0);
        gl::BindTexture(format, m_colorGrader.texture(grader::kOutput));

        // render window buffer
        m_defaultMethod.enable();
        m_defaultMethod.setPerspective(pl.perspective());
        m_quad.render();
    }
}

void world::spotLightShadowPass(const spotLight *const sl) {
    GLint depthMask;
    GLint depthFunc;

    gl::GetIntegerv(GL_DEPTH_WRITEMASK, &depthMask);
    gl::GetIntegerv(GL_DEPTH_FUNC, &depthFunc);

    if (depthMask != GL_TRUE)
        gl::DepthMask(GL_TRUE);
    if (depthFunc != GL_LEQUAL)
        gl::DepthFunc(GL_LEQUAL);

    // Bind and clear the shadow map
    m_spotLightShadowMap.update(r_smsize);
    m_spotLightShadowMap.bindWriting();
    gl::Clear(GL_DEPTH_BUFFER_BIT);

    // Calculate lighting matrix
    m::mat4 translate, rotate, project;
    translate.setTranslateTrans(-sl->position.x, -sl->position.y, -sl->position.z);
    rotate.setCameraTrans(sl->direction, m::vec3::yAxis);
    project.setSpotLightPerspectiveTrans(sl->cutOff, sl->radius);

    m_shadowMapMethod.enable();
    m_shadowMapMethod.setWVP(project * rotate * translate);

    // Draw the scene from the lights perspective into the shadow map
    gl::Viewport(0, 0, r_smsize, r_smsize);
    gl::BindVertexArray(vao);
    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
    gl::Viewport(0, 0, neoWidth(), neoHeight());

    m_final.bindWriting();

    gl::DepthMask(depthMask);
    gl::DepthFunc(depthFunc);
}

void world::render(const pipeline &pl, ::world *map) {
    cullPass(pl, map);
    occlusionPass(pl, map);
    geometryPass(pl, map);
    lightingPass(pl, map);
    forwardPass(pl, map);
    compositePass(pl, map);
}

}
