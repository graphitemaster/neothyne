#ifndef R_MODEL_HDR
#define R_MODEL_HDR
#include "model.h"

#include "r_geom.h"
#include "r_common.h"
#include "r_method.h"

#include "u_string.h"
#include "u_map.h"

namespace r {

struct pipeline;
struct texture2D;

struct geomMethod : method {
    geomMethod();

    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setWorld(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setSpecTextureUnit(int unit);
    void setDispTextureUnit(int unit);
    void setSpecPower(float power);
    void setSpecIntensity(float intensity);
    void setEyeWorldPos(const m::vec3 &position);
    void setParallax(float scale, float bias);
    void setBoneMats(size_t numJoints, const float *mats);

private:
    GLint m_WVPLocation;
    GLint m_worldLocation;
    GLint m_colorTextureUnitLocation;
    GLint m_normalTextureUnitLocation;
    GLint m_specTextureUnitLocation;
    GLint m_dispTextureUnitLocation;
    GLint m_specPowerLocation;
    GLint m_specIntensityLocation;
    GLint m_eyeWorldPositionLocation;
    GLint m_parallaxLocation;
    GLint m_boneMatsLocation;
};

inline geomMethod::geomMethod()
    : m_WVPLocation(-1)
    , m_worldLocation(-1)
    , m_colorTextureUnitLocation(-1)
    , m_normalTextureUnitLocation(-1)
    , m_specTextureUnitLocation(-1)
    , m_dispTextureUnitLocation(-1)
    , m_specPowerLocation(-1)
    , m_specIntensityLocation(-1)
    , m_eyeWorldPositionLocation(-1)
    , m_parallaxLocation(-1)
    , m_boneMatsLocation(-1)
{
}

struct geomMethods {
    static geomMethods &instance() {
        return m_instance;
    }

    bool init();
    geomMethod &operator[](size_t index);
    const geomMethod &operator[](size_t index) const;

private:
    geomMethods();
    geomMethods(const geomMethods &) = delete;
    void operator =(const geomMethods &) = delete;

    u::vector<geomMethod> m_geomMethods;
    bool m_initialized;
    static geomMethods m_instance;
};

inline geomMethod &geomMethods::operator[](size_t index) {
    return m_geomMethods[index];
}

inline const geomMethod &geomMethods::operator[](size_t index) const {
    return m_geomMethods[index];
}

struct material {
    material();

    int permute; // Geometry pass permutation for this material
    texture2D *diffuse;
    texture2D *normal;
    texture2D *spec;
    texture2D *displacement;
    bool specParams;
    float specPower;
    float specIntensity;
    float dispScale;
    float dispBias;

    void calculatePermutation(bool skeletal = false);
    geomMethod *bind(const r::pipeline &pl, const m::mat4 &rw, bool skeletal = false);
    bool load(u::map<u::string, texture2D*> &textures, const u::string &file, const u::string &basePath);
    bool upload();
};

struct model : geom {
    model();

    bool load(u::map<u::string, texture2D*> &textures, const u::string &file);
    bool upload();

    void render(const r::pipeline &pl, const m::mat4 &w);
    void render(); // GUI model rendering (diffuse only, single material, entire model)

    void animate(float curFrame);
    bool animated() const;

    m::vec3 scale;
    m::vec3 rotate;

    m::bbox bounds() const;

private:
    geomMethods *m_geomMethods;
    u::vector<material> m_materials;
    u::vector<::model::batch> m_batches;
    size_t m_indices;
    ::model m_model;
};

inline m::bbox model::bounds() const {
    return m_model.bounds();
}

}
#endif
