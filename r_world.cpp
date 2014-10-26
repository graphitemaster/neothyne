#include "r_world.h"

#include "u_algorithm.h"
#include "u_memory.h"
#include "u_misc.h"
#include "u_file.h"

#include "engine.h"

namespace r {
///! methods

///! Light Rendering Method
bool lightMethod::init(const char *vs, const char *fs) {
    if (!method::init())
        return false;

    if (gl::has(ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

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
    m_specTextureUnitLocation = getUniformLocation("gSpecMap");
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

void lightMethod::setSpecTextureUnit(int unit) {
    gl::Uniform1i(m_specTextureUnitLocation, unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, &position.x);
}

void lightMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
    gl::Uniform2f(m_screenFrustumLocation, project.nearp, project.farp);
}

void lightMethod::setDepthTextureUnit(int unit) {
    gl::Uniform1i(m_depthTextureUnitLocation, unit);
}

///! Directional Light Rendering Method
bool directionalLightMethod::init() {
    if (!lightMethod::init("shaders/dlight.vs", "shaders/dlight.fs"))
        return false;

    m_directionalLightLocation.color = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLightLocation.ambient = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLightLocation.diffuse = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLightLocation.direction = getUniformLocation("gDirectionalLight.direction");

    return true;
}

void directionalLightMethod::setDirectionalLight(const directionalLight &light) {
    m::vec3 direction = light.direction.normalized();
    gl::Uniform3fv(m_directionalLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_directionalLightLocation.ambient, light.ambient);
    gl::Uniform3fv(m_directionalLightLocation.direction, 1, &direction.x);
    gl::Uniform1f(m_directionalLightLocation.diffuse, light.diffuse);
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
    m_specPowerLocation = getUniformLocation("gSpecPower");
    m_specIntensityLocation = getUniformLocation("gSpecIntensity");

    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void geomMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void geomMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureUnitLocation, unit);
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

///! Depth Pass Method
bool depthMethod::init() {
    if (!method::init())
        return false;
    if (!addShader(GL_VERTEX_SHADER, "shaders/depthpass.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/depthpass.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    return true;
}

void depthMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

///! renderer
world::world() {
    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.90f;
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

    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        switch (rand() % 4) {
            case 0: m_billboards[kBillboardRail].add(places[i]); break;
            case 1: m_billboards[kBillboardLightning].add(places[i]); break;
            case 2: m_billboards[kBillboardRocket].add(places[i]); break;
            case 3: m_billboards[kBillboardShotgun].add(places[i]); break;
        }
    }
}

world::~world() {
    gl::DeleteBuffers(2, m_buffers);
    gl::DeleteVertexArrays(1, &m_vao);

    for (auto &it : m_textures2D)
        delete it.second;
}

bool world::loadMaterial(const kdMap &map, renderTextureBatch *batch) {
    u::string materialName = map.textures[batch->index].name;
    u::string diffuseName;
    u::string specName;
    u::string normalName = "<normal>";

    // Read the material
    auto fp = u::fopen(neoGamePath() + materialName + ".cfg", "r");
    if (!fp) {
        u::print("Failed to load material: %s\n", materialName);
        return false;
    }

    bool specParams = false;
    float specIntensity = 0;
    float specPower = 0;

    while (auto line = u::getline(fp)) {
        auto get = *line;
        auto split = u::split(get);
        auto key = split[0];
        auto value = split[1];
        if (key == "diffuse")
            diffuseName = "textures/" + value;
        else if (key == "normal")
            normalName += "textures/" + value;
        else if (key == "spec")
            specName = "textures/" + value;
        else if (key == "spec_power") {
            specPower = u::atof(value);
            specParams = true;
        } else if (key == "spec_intensity") {
            specIntensity = u::atof(value);
            specParams = true;
        }
    }

    auto loadTexture = [&](const u::string &ident, texture2D **store) {
        if (m_textures2D.find(ident) != m_textures2D.end()) {
            *store = m_textures2D[ident];
        } else {
            u::unique_ptr<texture2D> tex(new texture2D);
            if (tex->load(ident)) {
                auto release = tex.release();
                m_textures2D[ident] = release;
                *store = release;
            } else {
                *store = nullptr;
            }
        }
    };

    loadTexture(diffuseName, &batch->diffuse);
    loadTexture(normalName, &batch->normal);
    loadTexture(specName, &batch->spec);

    // If there is a specular map, silently drop the specular parameters
    if (batch->spec && specParams)
        specParams = false;

    batch->specParams = specParams;
    batch->specIntensity = specIntensity;
    batch->specPower = specPower;

    return true;
}

bool world::load(const kdMap &map) {
    // load skybox
    if (!m_skybox.load("textures/sky01"))
        return false;

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

    // load materials
    for (size_t i = 0; i < m_textureBatches.size(); i++)
        loadMaterial(map, &m_textureBatches[i]);

    m_vertices = u::move(map.vertices);
    u::print("[world] => loaded\n");
    return true;
}

bool world::upload(const m::perspectiveProjection &project) {
    // upload skybox
    if (!m_skybox.upload())
        neoFatal("failed to upload skybox");

    // upload billboards
    for (auto &it : m_billboards) {
        if (it.upload())
            continue;
        neoFatal("failed to upload billboard");
    }

    // upload textures
    for (auto &it : m_textureBatches) {
        if (it.diffuse && !it.diffuse->upload())
            neoFatal("failed to upload world textures");
        if (it.normal && !it.normal->upload())
            neoFatal("failed to upload world textures");
    }

    if (!m_directionalLightQuad.upload())
        neoFatal("failed to upload directional light quad");

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(kdBinVertex), &m_vertices[0], GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), GL_OFFSET(0));  // vertex
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), GL_OFFSET(3));  // normals
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), GL_OFFSET(6));  // texCoord
    gl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), GL_OFFSET(8));  // tangent
    gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), GL_OFFSET(11)); // w
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);
    gl::EnableVertexAttribArray(3);
    gl::EnableVertexAttribArray(4);

    // upload data
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_STATIC_DRAW);

    if (!m_depthMethod.init())
        neoFatal("failed to initialize depth pass method\n");

    // Init the shader permutations
    // 0 = pass-through shader (vertex normals)
    // 1 = diffuse only permutation
    // 2 = diffuse and normal permutation
    // 3 = diffuse and spec permutation
    // 4 = diffuse and spec param permutation
    // 5 = diffuse and normal and spec permutation
    // 6 = diffuse and normal and spec param permutation
    if (!m_geomMethods[0].init())
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[1].init({"USE_DIFFUSE"}))
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[2].init({"USE_DIFFUSE", "USE_NORMALMAP"}))
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[3].init({"USE_DIFFUSE", "USE_SPECMAP"}))
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[4].init({"USE_DIFFUSE", "USE_SPECPARAMS"}))
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[5].init({"USE_DIFFUSE", "USE_NORMALMAP", "USE_SPECMAP"}))
        neoFatal("failed to initialize geometry rendering method\n");
    if (!m_geomMethods[6].init({"USE_DIFFUSE", "USE_NORMALMAP", "USE_SPECPARAMS"}))
        neoFatal("failed to initialize geometry rendering method\n");

    if (!m_directionalLightMethod.init())
        neoFatal("failed to initialize directional light rendering method");

    // setup g-buffer
    if (!m_gBuffer.init(project))
        neoFatal("failed to initialize G-buffer");

    m_directionalLightMethod.enable();
    m_directionalLightMethod.setColorTextureUnit(gBuffer::kDiffuse);
    m_directionalLightMethod.setNormalTextureUnit(gBuffer::kNormal);
    m_directionalLightMethod.setSpecTextureUnit(gBuffer::kSpec);
    m_directionalLightMethod.setDepthTextureUnit(gBuffer::kDepth);

    // Setup the shader permutations
    // 0 = pass-through shader (vertex normals)
    // 1 = diffuse only permutation
    // 2 = diffuse and normal permutation
    // 3 = diffuse and spec permutation
    // 4 = diffuse and spec param permutation
    // 5 = diffuse and normal and spec permutation
    // 6 = diffuse and normal and spec param permutation
    m_geomMethods[0].enable();

    m_geomMethods[1].enable();
    m_geomMethods[1].setColorTextureUnit(0);

    m_geomMethods[2].enable();
    m_geomMethods[2].setColorTextureUnit(0);
    m_geomMethods[2].setNormalTextureUnit(1);

    m_geomMethods[3].enable();
    m_geomMethods[3].setColorTextureUnit(0);
    m_geomMethods[3].setSpecTextureUnit(1);

    m_geomMethods[4].enable();
    m_geomMethods[4].setColorTextureUnit(0);

    m_geomMethods[5].enable();
    m_geomMethods[5].setColorTextureUnit(0);
    m_geomMethods[5].setNormalTextureUnit(1);
    m_geomMethods[5].setSpecTextureUnit(2);

    m_geomMethods[6].enable();
    m_geomMethods[6].setColorTextureUnit(0);
    m_geomMethods[6].setNormalTextureUnit(1);

    u::print("[world] => uploaded\n");
    return true;
}

void world::geometryPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    m_gBuffer.bindWriting();

    // Only the geometry pass should update the depth buffer
    gl::DepthMask(GL_TRUE);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Only the geometry pass should update the depth buffer
    gl::Enable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);

    // Update the shader permuations
    for (size_t i = 0; i < sizeof(m_geomMethods)/sizeof(*m_geomMethods); i++) {
        m_geomMethods[i].enable();
        m_geomMethods[i].setWVP(p.getWVPTransform());
        m_geomMethods[i].setWorld(p.getWorldTransform());
    }

    // Render the world
    gl::BindVertexArray(m_vao);
    for (auto &it : m_textureBatches) {
        if (it.specParams) {
            // Specularity parameter permutation (global spec map)
            if (it.normal) {
                // diffuse + normal + specparams
                m_geomMethods[6].enable();
                it.diffuse->bind(GL_TEXTURE0);
                it.normal->bind(GL_TEXTURE1);
            } else {
                // diffuse + specparams
                m_geomMethods[4].enable();
                it.diffuse->bind(GL_TEXTURE0);
                m_geomMethods[4].setSpecIntensity(it.specIntensity);
                m_geomMethods[4].setSpecPower(it.specPower);
            }
        } else {
            if (it.diffuse) {
                if (it.normal) {
                    if (it.spec) {
                        // diffuse + normal + spec
                        m_geomMethods[5].enable();
                        it.diffuse->bind(GL_TEXTURE0);
                        it.normal->bind(GL_TEXTURE1);
                        it.spec->bind(GL_TEXTURE2);
                    } else {
                        // diffuse + normal
                        m_geomMethods[2].enable();
                        it.diffuse->bind(GL_TEXTURE0);
                        it.normal->bind(GL_TEXTURE1);
                    }
                } else {
                    if (it.spec) {
                        // diffuse + spec
                        m_geomMethods[3].enable();
                        it.diffuse->bind(GL_TEXTURE0);
                        it.spec->bind(GL_TEXTURE1);
                    } else {
                        // diffuse
                        m_geomMethods[1].enable();
                        it.diffuse->bind(GL_TEXTURE0);
                    }
                }
            } else {
                // null
                m_geomMethods[0].enable();
            }
        }
        gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(GLuint) * it.start));
    }

    // The depth buffer is populated and the stencil pass will require it.
    // However, it will not write to it.
    gl::DepthMask(GL_FALSE);
    gl::Disable(GL_DEPTH_TEST);
}

void world::beginLightPass() {
    gl::Enable(GL_BLEND);
    gl::BlendEquation(GL_FUNC_ADD);
    gl::BlendFunc(GL_ONE, GL_ONE);

    m_gBuffer.bindReading();
    gl::Clear(GL_COLOR_BUFFER_BIT);
}

void world::directionalLightPass(const rendererPipeline &pipeline) {
    const m::perspectiveProjection &project = pipeline.getPerspectiveProjection();
    m_directionalLightMethod.enable();

    m_directionalLightMethod.setDirectionalLight(m_directionalLight);
    m_directionalLightMethod.setPerspectiveProjection(project);

    m_directionalLightMethod.setEyeWorldPos(pipeline.getPosition());

    m::mat4 wvp;
    wvp.loadIdentity();

    rendererPipeline p = pipeline;
    m_directionalLightMethod.setWVP(wvp);
    m_directionalLightMethod.setInverse(p.getInverseTransform());
    m_directionalLightQuad.render();
}

void world::depthPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;
    m_depthMethod.enable();
    m_depthMethod.setWVP(p.getWVPTransform());

    gl::Enable(GL_DEPTH_TEST); // make sure depth testing is enabled
    gl::Clear(GL_DEPTH_BUFFER_BIT); // clear

    // do a depth pass
    gl::BindVertexArray(m_vao);
    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
}

void world::depthPrePass(const rendererPipeline &pipeline) {
    gl::DepthMask(GL_TRUE);
    gl::DepthFunc(GL_LESS);
    gl::ColorMask(0.0f, 0.0f, 0.0f, 0.0f);

    depthPass(pipeline);

    gl::DepthFunc(GL_LEQUAL);
    gl::DepthMask(GL_FALSE);
    gl::ColorMask(1.0f, 1.0f, 1.0f, 1.0f);
}

void world::render(const rendererPipeline &pipeline) {
    m_gBuffer.update(pipeline.getPerspectiveProjection());

    // depth pre pass
    depthPrePass(pipeline);

    // geometry pass
    geometryPass(pipeline);

    // begin lighting pass
    beginLightPass();

    // directional light pass
    directionalLightPass(pipeline);

    // Now standard forward rendering
    //m_gBuffer.bindReading();
    gl::Enable(GL_DEPTH_TEST);
    m_skybox.render(pipeline);

    gl::DepthMask(GL_TRUE);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto &it : m_billboards)
        it.render(pipeline);
}

}