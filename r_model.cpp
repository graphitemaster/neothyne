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
    if (!method::init("geometry"))
        return false;

    for (const auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/geom.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/geom.fs"))
        return false;

    if (!finalize({ "position", "normal", "texCoord", "tangent", "weights", "bones" },
                  { "diffuseOut", "normalOut" }))
    {
        return false;
    }

    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_world = getUniform("gWorld", uniform::kMat4);
    m_colorTextureUnit = getUniform("gColorMap", uniform::kSampler);
    m_normalTextureUnit = getUniform("gNormalMap", uniform::kSampler);
    m_specTextureUnit = getUniform("gSpecMap", uniform::kSampler);
    m_dispTextureUnit = getUniform("gDispMap", uniform::kSampler);
    m_specPower = getUniform("gSpecPower", uniform::kFloat);
    m_specIntensity = getUniform("gSpecIntensity", uniform::kFloat);
    m_eyeWorldPosition = getUniform("gEyeWorldPosition", uniform::kVec3);
    m_parallax = getUniform("gParallax", uniform::kVec2);
    m_boneMats = getUniform("gBoneMats", uniform::kMat3x4Array);
    m_animOffset = getUniform("gAnimOffset", uniform::kInt2);
    m_animFlip = getUniform("gAnimFlip", uniform::kVec2);
    m_animScale = getUniform("gAnimScale", uniform::kVec2);

    post();
    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    m_world->set(worldInverse);
}

void geomMethod::setEyeWorldPos(const m::vec3 &position) {
    m_eyeWorldPosition->set(position);
}

void geomMethod::setParallax(float scale, float bias) {
    m_parallax->set(m::vec2(scale, bias));
}

void geomMethod::setColorTextureUnit(int unit) {
    m_colorTextureUnit->set(unit);
}

void geomMethod::setNormalTextureUnit(int unit) {
    m_normalTextureUnit->set(unit);
}

void geomMethod::setDispTextureUnit(int unit) {
    m_dispTextureUnit->set(unit);
}

void geomMethod::setSpecTextureUnit(int unit) {
    m_specTextureUnit->set(unit);
}

void geomMethod::setSpecIntensity(float intensity) {
    m_specIntensity->set(intensity);
}

void geomMethod::setSpecPower(float power) {
    m_specPower->set(power);
}

void geomMethod::setBoneMats(size_t numJoints, const float *mats) {
    m_boneMats->set(numJoints, mats);
}

void geomMethod::setAnimation(int x, int y, float flipu, float flipv, float w, float h) {
    m_animOffset->set(x, y);
    m_animFlip->set(m::vec2(flipu, flipv));
    m_animScale->set(m::vec2(w, h));
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
    kGeomPermSkeletal       = 1 << 5,
    kGeomPermAnimated       = 1 << 6
};

///! Geometry shading permutation singleton
static const geomPermutation kGeomPermutations[] = {
    // Null permutation
    { 0,                                                                                                                       -1, -1, -1, -1 },
    // Geometry permutations (static)
    { kGeomPermDiffuse,                                                                                                         0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                                                                    0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap,                                                                                      0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams,                                                                                   0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap,                                                                                    0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap,                                                                0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams,                                                             0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax,                                                               0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax,                                         0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax,                                         0,  1, -1,  2 },
    // Geometry permutations (animated)
    { kGeomPermDiffuse | kGeomPermAnimated,                                                                                     0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermAnimated,                                                               0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap    | kGeomPermAnimated,                                                               0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams | kGeomPermAnimated,                                                               0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermAnimated,                                                               0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermAnimated,                                         0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermAnimated,                                         0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax   | kGeomPermAnimated,                                         0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax | kGeomPermAnimated,                     0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax | kGeomPermAnimated,                     0,  1, -1,  2 },
    // Skeletal permutations (static)
    { kGeomPermSkeletal,                                                                                                       -1, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSkeletal,                                                                                     0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal,                                                               0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap    | kGeomPermSkeletal,                                                               0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams | kGeomPermSkeletal,                                                               0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal,                                                               0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermSkeletal,                                         0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermSkeletal,                                         0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax   | kGeomPermSkeletal,                                         0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax | kGeomPermSkeletal,                     0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax | kGeomPermSkeletal,                     0,  1, -1,  2 },
    // Skeletal permutations (animated)
    { kGeomPermDiffuse | kGeomPermAnimated,                                                                                     0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSkeletal   | kGeomPermAnimated,                                                               0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal   | kGeomPermAnimated,                                         0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecMap    | kGeomPermSkeletal   | kGeomPermAnimated,                                         0, -1,  1, -1 },
    { kGeomPermDiffuse | kGeomPermSpecParams | kGeomPermSkeletal   | kGeomPermAnimated,                                         0, -1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSkeletal   | kGeomPermAnimated,                                         0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermSkeletal | kGeomPermAnimated,                     0,  1,  2, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermSkeletal | kGeomPermAnimated,                     0,  1, -1, -1 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermParallax   | kGeomPermSkeletal | kGeomPermAnimated,                     0,  1, -1,  2 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecMap    | kGeomPermParallax | kGeomPermSkeletal | kGeomPermAnimated, 0,  1,  2,  3 },
    { kGeomPermDiffuse | kGeomPermNormalMap  | kGeomPermSpecParams | kGeomPermParallax | kGeomPermSkeletal | kGeomPermAnimated, 0,  1, -1,  2 }
};

static const char *kGeomPermutationNames[] = {
    "USE_DIFFUSE",
    "USE_NORMALMAP",
    "USE_SPECMAP",
    "USE_SPECPARAMS",
    "USE_PARALLAX",
    "USE_SKELETAL",
    "USE_ANIMATION"
};

///! Singleton representing all the possible geometry methods (used by model and world.)
geomMethods::geomMethods()
    : m_geomMethods(nullptr)
    , m_initialized(false)
{
}

void geomMethods::release() {
    delete m_geomMethods;
}

bool geomMethods::init() {
    if (m_initialized)
        return true;

    // geometry shader permutations
    static const size_t kGeomCount = sizeof kGeomPermutations / sizeof *kGeomPermutations;
    m_geomMethods = new u::vector<geomMethod>(kGeomCount);
    for (size_t i = 0; i < kGeomCount; i++) {
        const auto &p = kGeomPermutations[i];
        if (!(*m_geomMethods)[i].init(generatePermutation(kGeomPermutationNames, p)))
            return false;
        (*m_geomMethods)[i].enable();
        if (p.color  != -1) (*m_geomMethods)[i].setColorTextureUnit(p.color);
        if (p.normal != -1) (*m_geomMethods)[i].setNormalTextureUnit(p.normal);
        if (p.spec   != -1) (*m_geomMethods)[i].setSpecTextureUnit(p.spec);
        if (p.disp   != -1) (*m_geomMethods)[i].setDispTextureUnit(p.disp);
    }

    return m_initialized = true;
}

bool geomMethods::reload() {
    for (auto &it : *m_geomMethods) {
        if (it.reload())
            continue;
        return false;
    }
    return true;
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
    , m_animFrameWidth(0)
    , m_animFrameHeight(0)
    , m_animFramerate(0)
    , m_animFrames(0)
    , m_animFrame(0)
    , m_animFramesPerRow(0)
    , m_animWidth(0.0f)
    , m_animHeight(0.0f)
    , m_animFlipU(false)
    , m_animFlipV(false)
    , m_animMillis(0)
    , m_geomMethods(&geomMethods::instance())
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

    int32_t colorized = -1;

    for (u::string get; u::getline(fp, get); ) {
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
        } else if (key == "animation" && split.size() > 4) {
            m_animFrameWidth = u::atoi(split[1]);
            m_animFrameHeight = u::atoi(split[2]);
            m_animFramerate = u::atoi(split[3]);
            m_animFrames = u::atoi(split[4]);
            if (split.size() > 5) m_animFlipU = u::atoi(split[5]);
            if (split.size() > 6) m_animFlipV = u::atoi(split[6]);
        } else if (key == "colorize" && split.size() == 2) {
            colorized = strtol(split[1].c_str(), nullptr, 16);
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
                if (colorized != -1) {
                    u::print("[material] => `%s' colorized with 0x%08X\n", ident, colorized);
                    (*store)->colorize(colorized);
                }
            } else {
                // Make it a "no texture" texture
                auto notex = textures.find("textures/notex");
                if (notex == textures.end()) {
                    u::unique_ptr<texture2D> tex(new texture2D);
                    if (tex->load("textures/notex")) {
                        auto release = tex.release();
                        textures["textures/notex"] = release;
                        *store = release;
                    } else {
                        neoFatal("failed to load \"no texture\" texture!");
                    }
                } else {
                    *store = notex->second;
                }
            }
        }
    };

    loadTexture(diffuseName, &diffuse);
    loadTexture(normalName, &normal);
    loadTexture(specName, &spec);
    loadTexture(displacementName, &displacement);

    // Sanitize animated inputs
    auto checkSize = [this, &fileName](texture2D *tex) {
        if (tex && m_animFrameWidth*m_animFramesPerRow > int(tex->width())) {
            u::print("[material] => `%s' animation frame width and frames per row exceeds animation texture width\n", fileName);
            return false;
        }
        return true;
    };

    if (m_animFrames && diffuse) {
        if (m_animFrameWidth <= 0 || m_animFrameHeight <= 0 ||
            m_animFramerate <= 0 || m_animFrames <= 0)
        {
            u::print("[material] => `%s' invalid animation sequence\n", fileName);
            return false;
        }

        if (!checkSize(diffuse)) return false;
        if (!checkSize(normal)) return false;
        if (!checkSize(spec)) return false;
        if (!checkSize(displacement)) return false;

        m_animFramesPerRow = diffuse->width() / m_animFrameWidth;
        m_animWidth = float(m_animFrameWidth) / diffuse->width();
        m_animHeight = float(m_animFrameHeight) / diffuse->height();

        if (m_animFrames / m_animFramesPerRow > int(diffuse->height()) / m_animFrameHeight) {
            u::print("[material] => `%s' frame-count exceeds the geometry of the animation sequence\n", fileName);
            return false;
        }
        if (m_animFramerate > m_animFrames) {
            u::print("[material] => `%s' frame-rate exceeds the amount of frames in animation sequence\n", fileName);
            return false;
        }
    }

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
    if (!m_geomMethods->init())
        return false;
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
    if (m_animFrames)
        p |= kGeomPermAnimated;
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
    auto &method = (*m_geomMethods)[permute];
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
    if (m_animFrames) {
        const float mspf = 1.0f / (float(m_animFramerate) / 1000.0f);
        if (pl.time() - m_animMillis >= mspf) {
            m_animFrame = (m_animFrame + 1) % m_animFrames;
            method.setAnimation((m_animFrame % m_animFramesPerRow),
                                (m_animFrame / m_animFramesPerRow),
                                (m_animFlipU ? -1.0f : 1.0f),
                                (m_animFlipV ? -1.0f : 1.0f),
                                m_animWidth,
                                m_animHeight);
            m_animMillis = pl.time();
        }
    }
    return &method;
}

///! Model Loading and Rendering
model::model()
    : m_geomMethods(&geomMethods::instance())
    , m_indices(0)
    , m_half(false)
{
}

bool model::load(u::map<u::string, texture2D*> &textures, const u::string &file) {
    // Open the model file and look for a model configuration
    u::file fp = u::fopen(neoGamePath() + file + ".cfg", "r");
    if (!fp)
        return false;

    u::vector<u::string> animNames;
    u::vector<u::string> materialNames;
    u::vector<u::string> materialFiles;
    u::string name;
    for (u::string line; u::getline(fp, line); ) {
        const auto split = u::split(line);
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
        } else if (split[0] == "material") {
            if (split.size() > 2) {
                materialNames.push_back(split[1]);
                materialFiles.push_back(split[2]);
            } else {
                u::print("[model] => invalid use of `material' key\n");
                return false;
            }
        } else if (split[0] == "half") {
            m_half = !!u::atoi(split[1]);
        } else if (split[0] == "anim") {
            animNames.push_back(split[1]);
        }
    }

    // Now use that to load the mesh
    if (!m_model.load("models/" + name, animNames))
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
            fileName = u::format("models/%s", materialFiles[i]);
            if (!m_materials[i].load(textures, fileName, "models/"))
                return false;
        }

        const auto &meshNames = m_model.meshNames();
        if (materialNames.size() != meshNames.size()) {
            u::print("[model] => config contains %s materials than meshes\n",
                materialNames.size() > meshNames.size() ? "more" : "less");
            return false;
        }

        // Resolve material indices
        for (size_t i = 0; i < materialNames.size(); i++) {
            auto find = u::find(meshNames.begin(), meshNames.end(), materialNames[i]);
            if (find == meshNames.end()) {
                u::print("[model] => config contains `%s' material but model doesn't\n",
                    materialNames[i]);
                return false;
            }
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

    const bool useHalf = (m_half || m_model.isHalf()) && gl::has(gl::ARB_half_float_vertex);

    if (useHalf && !m_model.isHalf())
        m_model.makeHalf();
    if (!useHalf && m_model.isHalf())
        m_model.makeSingle();

    const char *precision = nullptr;
    const char *state = nullptr;
    if (m_model.animated()) {
        state = "animated";
        if (useHalf) {
            const auto &vertices = m_model.animHalfVertices();
            static const mesh::animHalfVertex *vert = nullptr;
            gl::BufferData(GL_ARRAY_BUFFER, sizeof *vert * vertices.size(), &vertices[0], GL_STATIC_DRAW);
            gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT,    GL_FALSE, sizeof *vert, &vert->position); // vertex
            gl::VertexAttribPointer(1, 3, GL_HALF_FLOAT,    GL_FALSE, sizeof *vert, &vert->normal); // normals
            gl::VertexAttribPointer(2, 2, GL_HALF_FLOAT,    GL_FALSE, sizeof *vert, &vert->coordinate); // texCoord
            gl::VertexAttribPointer(3, 4, GL_HALF_FLOAT,    GL_FALSE, sizeof *vert, &vert->tangent); // tangent
            gl::VertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof *vert, &vert->blendWeight); // blend weight
            gl::VertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof *vert, &vert->blendIndex); // blend index
            precision = "half";
        } else {
            const auto &vertices = m_model.animVertices();
            static const mesh::animVertex *vert = nullptr;
            gl::BufferData(GL_ARRAY_BUFFER, sizeof *vert * vertices.size(), &vertices[0], GL_STATIC_DRAW);
            gl::VertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof *vert, &vert->position); // vertex
            gl::VertexAttribPointer(1, 3, GL_FLOAT,         GL_FALSE, sizeof *vert, &vert->normal); // normals
            gl::VertexAttribPointer(2, 2, GL_FLOAT,         GL_FALSE, sizeof *vert, &vert->coordinate); // texCoord
            gl::VertexAttribPointer(3, 4, GL_FLOAT,         GL_FALSE, sizeof *vert, &vert->tangent); // tangent
            gl::VertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof *vert, &vert->blendWeight); // blend weight
            gl::VertexAttribPointer(5, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof *vert, &vert->blendIndex); // blend index
            precision = "single";
        }
        gl::EnableVertexAttribArray(0);
        gl::EnableVertexAttribArray(1);
        gl::EnableVertexAttribArray(2);
        gl::EnableVertexAttribArray(3);
        gl::EnableVertexAttribArray(4);
        gl::EnableVertexAttribArray(5);
    } else {
        state = "static";
        if (useHalf) {
            const auto &vertices = m_model.basicHalfVertices();
            static const mesh::basicHalfVertex *vert = nullptr;
            gl::BufferData(GL_ARRAY_BUFFER, sizeof *vert * vertices.size(), &vertices[0], GL_STATIC_DRAW);
            gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, sizeof *vert, &vert->position);
            gl::VertexAttribPointer(1, 3, GL_HALF_FLOAT, GL_FALSE, sizeof *vert, &vert->normal);
            gl::VertexAttribPointer(2, 2, GL_HALF_FLOAT, GL_FALSE, sizeof *vert, &vert->coordinate);
            gl::VertexAttribPointer(3, 4, GL_HALF_FLOAT, GL_FALSE, sizeof *vert, &vert->tangent);
            precision = "half";
        } else {
            const auto &vertices = m_model.basicVertices();
            static const mesh::basicVertex *vert = nullptr;
            gl::BufferData(GL_ARRAY_BUFFER, sizeof *vert * vertices.size(), &vertices[0], GL_STATIC_DRAW);
            gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof *vert, &vert->position);
            gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof *vert, &vert->normal);
            gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof *vert, &vert->coordinate);
            gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof *vert, &vert->tangent);
            precision = "single";
        }
        gl::EnableVertexAttribArray(0);
        gl::EnableVertexAttribArray(1);
        gl::EnableVertexAttribArray(2);
        gl::EnableVertexAttribArray(3);
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices * sizeof indices[0], &indices[0], GL_STATIC_DRAW);

    // Upload materials
    for (auto &mat : m_materials)
        if (!mat.upload())
            return false;

    const size_t materials = m_model.meshNames().size();
    u::print("[model] => loaded %s model `%s' (containing %zu %s) using %s-precision float\n",
        state, u::fixPath(m_model.name()), materials, materials > 1 ? "materials" : "material", precision);

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
