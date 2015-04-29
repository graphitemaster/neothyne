#include <assert.h>

#include "r_model.h"
#include "r_texture.h"
#include "r_pipeline.h"

#include "engine.h"

#include "u_misc.h"
#include "u_file.h"

#include "cvar.h"

namespace r {

///! Geometry rendering method (used for models and the world.)
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

// Generate the list of permutation names for the shader
template <typename T, size_t N, typename U>
static inline u::vector<const char *> generatePermutation(const T(&list)[N], const U &p) {
    u::vector<const char *> permutes;
    for (size_t i = 0; i < N; i++)
        if (p.permute & (1 << i))
            permutes.push_back(list[i]);
    return permutes;
}

struct geomPermutation {
    int permute;    // flags of the permutations
    int color;      // color texture unit (or -1 if not to be used)
    int normal;     // normal texture unit (or -1 if not to be used)
    int spec;       // ...
    int disp;       // ...
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

///! Geometry shading permutation singleton
static const geomPermutation kGeomPermutations[] = {
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

static const char *kGeomPermutationNames[] = {
    "USE_DIFFUSE",
    "USE_NORMALMAP",
    "USE_SPECMAP",
    "USE_SPECPARAMS",
    "USE_PARALLAX",
    "USE_SKELETAL"
};

///! Singleton representing all the possible geometry methods (used by model and world.)
geomMethods::geomMethods()
    : m_initialized(false)
{
}

bool geomMethods::init() {
    if (m_initialized)
        return true;

    // geometry shader permutations
    static const size_t geomCount = sizeof(kGeomPermutations)/sizeof(kGeomPermutations[0]);
    m_geomMethods.resize(geomCount);
    for (size_t i = 0; i < geomCount; i++) {
        const auto &p = kGeomPermutations[i];
        if (!m_geomMethods[i].init(generatePermutation(kGeomPermutationNames, p)))
            return false;
        m_geomMethods[i].enable();
        if (p.color  != -1) m_geomMethods[i].setColorTextureUnit(p.color);
        if (p.normal != -1) m_geomMethods[i].setNormalTextureUnit(p.normal);
        if (p.spec   != -1) m_geomMethods[i].setSpecTextureUnit(p.spec);
        if (p.disp   != -1) m_geomMethods[i].setDispTextureUnit(p.disp);
    }

    return m_initialized = true;
}

geomMethods geomMethods::m_instance;

///! Model Material Loading (used by model and world.)
material::material()
    : permute(0)
    , diffuse(nullptr)
    , normal(nullptr)
    , spec(nullptr)
    , displacement(nullptr)
    , specParams(false)
    , specPower(0.0f)
    , specIntensity(0.0f)
    , dispScale(0.0f)
    , dispBias(0.0f)
{
}

bool material::load(u::map<u::string, texture2D*> &textures, const u::string &materialName, const u::string &basePath) {
    u::string diffuseName;
    u::string specName;
    u::string normalName;
    u::string displacementName;

    // Read the material
    const u::string fileName = neoGamePath() + materialName + ".cfg";
    auto fp = u::fopen(fileName, "r");
    if (!fp) {
        u::print("Failed to load material: %s (%s)\n", materialName, fileName);
        return false;
    }

    bool specParams = false;
    float specIntensity = 0;
    float specPower = 0;
    float dispScale = 0;
    float dispBias = 0;

    while (auto line = u::getline(fp)) {
        auto get = *line;
        auto split = u::split(get);
        if (split.size() < 2)
            continue;
        auto key = split[0];
        auto value = split[1];
        if (key == "diffuse")
            diffuseName = basePath + value;
        else if (key == "normal")
            normalName = "<normal>" + basePath + value;
        else if (key == "displacement")
            displacementName = "<grey>" + basePath + value;
        else if (key == "spec")
            specName = basePath + value;
        else if (key == "specparams") {
            specPower = u::atof(value);
            if (split.size() > 2)
                specIntensity = u::atof(split[2]);
            specParams = true;
        } else if (key == "parallax") {
            dispScale = u::atof(value);
            if (split.size() > 2)
                dispBias = u::atof(split[2]);
        }
    }

    auto loadTexture = [&](const u::string &ident, texture2D **store) {
        if (ident.empty())
            return;
        if (textures.find(ident) != textures.end()) {
            *store = textures[ident];
        } else {
            u::unique_ptr<texture2D> tex(new texture2D);
            if (tex->load(ident)) {
                auto release = tex.release();
                textures[ident] = release;
                *store = release;
            } else {
                *store = nullptr;
            }
        }
    };

    loadTexture(diffuseName, &diffuse);
    loadTexture(normalName, &normal);
    loadTexture(specName, &spec);
    loadTexture(displacementName, &displacement);

    // If there is a specular map, silently drop the specular parameters
    if (spec && specParams)
        specParams = false;

    this->specParams = specParams;
    this->specIntensity = specIntensity / 2.0f;
    this->specPower = m::log2(specPower) / 8.0f;
    this->dispScale = dispScale;
    this->dispBias = dispBias;

    return true;
}

bool material::upload() {
    if (diffuse && !diffuse->upload())
        return false;
    if (normal && !normal->upload())
        return false;
    if (spec && !spec->upload())
        return false;
    if (displacement && !displacement->upload())
        return false;
    return true;
}

void material::calculatePermutation(bool skeletal) {
    var<int> &spec_ = varGet<int>("r_spec");
    var<int> &parallax_ = varGet<int>("r_parallax");

    int p = 0;
    if (skeletal)
        p |= kGeomPermSkeletal;
    if (diffuse)
        p |= kGeomPermDiffuse;
    if (normal)
        p |= kGeomPermNormalMap;
    if (spec && spec_.get())
        p |= kGeomPermSpecMap;
    if (displacement && parallax_.get())
        p |= kGeomPermParallax;
    if (specParams && spec_.get())
        p |= kGeomPermSpecParams;
    for (auto &it : kGeomPermutations) {
        if (it.permute == p) {
            permute = &it - kGeomPermutations;
            return;
        }
    }
}

geomMethod *material::bind(const r::pipeline &pl, const m::mat4 &rw, bool skeletal) {
    calculatePermutation(skeletal);
    r::pipeline p = pl;
    auto &permutation = kGeomPermutations[permute];
    auto &method = geomMethods::instance()[permute];
    method.enable();
    method.setWVP(p.projection() * p.view() * p.world());
    method.setWorld(rw);
    if (permutation.permute & kGeomPermParallax) {
        method.setEyeWorldPos(p.position());
        method.setParallax(dispScale, dispBias);
    }
    if (permutation.permute & kGeomPermSpecParams) {
        method.setSpecIntensity(specIntensity);
        method.setSpecPower(specPower);
    }
    if (permutation.permute & kGeomPermDiffuse)
        diffuse->bind(GL_TEXTURE0 + permutation.color);
    if (permutation.permute & kGeomPermNormalMap)
        normal->bind(GL_TEXTURE0 + permutation.normal);
    if (permutation.permute & kGeomPermSpecMap)
        spec->bind(GL_TEXTURE0 + permutation.spec);
    if (permutation.permute & kGeomPermParallax)
        displacement->bind(GL_TEXTURE0 + permutation.disp);
    return &method;
}

///! Model Loading and Rendering
model::model()
    : m_geomMethods(&geomMethods::instance())
    , m_indices(0)
{
}

bool model::load(u::map<u::string, texture2D*> &textures, const u::string &file) {
    // Open the model file and look for a model configuration
    u::file fp = u::fopen(neoGamePath() + file + ".cfg", "r");
    if (!fp)
        return false;

    u::vector<u::string> materialNames;
    u::vector<u::string> materialFiles;
    u::string name;
    while (auto getline = u::getline(fp)) {
        auto split = u::split(*getline);
        if (split.size() < 2)
            continue;
        if (split[0] == "model" && name.empty())
            name = u::move(split[1]);
        else if (split[0] == "scale") {
            scale.x = u::atof(split[1]);
            if (split.size() > 2) scale.y = u::atof(split[2]);
            if (split.size() > 3) scale.z = u::atof(split[3]);
        } else if (split[0] == "rotate") {
            rotate.x = u::atof(split[1]);
            if (split.size() > 2) rotate.y = u::atof(split[2]);
            if (split.size() > 3) rotate.z = u::atof(split[3]);
        } else if (split[0] == "material" && split.size() > 2) {
            materialNames.push_back(split[1]);
            materialFiles.push_back(split[2]);
        }
    }

    // Now use that to load the mesh
    if (!m_model.load("models/" + name))
        return false;

    // Copy the model batches
    m_batches = m_model.batches();

    // If there are no material definitions in the model configuration file it more
    // than likely implies that the model only has one material, which would be
    // inline with the model configuration file.
    if (materialNames.empty()) {
        m_materials.resize(1);
        if (!m_materials[0].load(textures, file, "models/"))
            return false;
        // Model only has one batch. Therefor the material index for it will be 0
        m_batches[0].material = 0;
    } else {
        m_materials.resize(materialNames.size());
        u::string fileName;
        for (size_t i = 0; i < materialNames.size(); i++) {
            fileName = u::format("models%c%s", u::kPathSep, materialFiles[i]);
            if (!m_materials[i].load(textures, fileName, "models/"))
                return false;
        }
        const auto &meshNames = m_model.meshNames();
        if (materialNames.size() > meshNames.size())
            u::print("[model] => config contains more materials than model contains meshes\n");
        // Resolve material indices
        for (size_t i = 0; i < meshNames.size(); i++) {
            if (meshNames[i] == materialNames[i])
                m_batches[i].material = i;
        }
    }

    return true;
}

bool model::upload() {
    geom::upload();

    const auto &indices = m_model.indices();
    m_indices = indices.size();

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (!m_geomMethods->init())
        return false;

    if (m_model.animated()) {
        const auto &vertices = m_model.animVertices();
        mesh::animVertex *vert = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, sizeof(mesh::animVertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof(mesh::animVertex), &vert->position); // vertex
        gl::VertexAttribPointer(1, 3, GL_FLOAT,         GL_FALSE, sizeof(mesh::animVertex), &vert->normal); // normals
        gl::VertexAttribPointer(2, 2, GL_FLOAT,         GL_FALSE, sizeof(mesh::animVertex), &vert->coordinate); // texCoord
        gl::VertexAttribPointer(3, 4, GL_FLOAT,         GL_FALSE, sizeof(mesh::animVertex), &vert->tangent); // tangent
        gl::VertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(mesh::animVertex), &vert->blendWeight); // blend weight
        gl::VertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(mesh::animVertex), &vert->blendIndex); // blend index
        gl::EnableVertexAttribArray(0);
        gl::EnableVertexAttribArray(1);
        gl::EnableVertexAttribArray(2);
        gl::EnableVertexAttribArray(3);
        gl::EnableVertexAttribArray(4);
        gl::EnableVertexAttribArray(5);
    } else {
        const auto &vertices = m_model.basicVertices();
        mesh::basicVertex *vert = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, sizeof(mesh::basicVertex) * vertices.size(), &vertices[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::basicVertex), &vert->position); // vertex
        gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(mesh::basicVertex), &vert->normal); // normals
        gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(mesh::basicVertex), &vert->coordinate); // texCoord
        gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(mesh::basicVertex), &vert->tangent); // tangent
        gl::EnableVertexAttribArray(0);
        gl::EnableVertexAttribArray(1);
        gl::EnableVertexAttribArray(2);
        gl::EnableVertexAttribArray(3);
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);

    // Upload materials
    for (auto &mat : m_materials)
        if (!mat.upload())
            return false;

    return true;
}

void model::render(const r::pipeline &pl, const m::mat4 &w) {
    gl::BindVertexArray(vao);

    if (animated()) { // Hoisted invariant out of loop because the compiler fails too
        for (const auto &it : m_batches) {
            auto *method = m_materials[it.material].bind(pl, w, true);
            method->setBoneMats(m_model.joints(), m_model.bones());
            gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT, it.offset);
        }
    } else {
        for (const auto &it : m_batches) {
            m_materials[it.material].bind(pl, w, false);
            gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT, it.offset);
        }
    }
}

void model::render() {
    gl::BindVertexArray(vao);
    m_materials[0].diffuse->bind(GL_TEXTURE0);
    gl::DrawElements(GL_TRIANGLES, m_batches[0].count, GL_UNSIGNED_INT, 0);
}

void model::animate(float curFrame) {
    m_model.animate(curFrame);
}

bool model::animated() const {
    return m_model.animated();
}

}
