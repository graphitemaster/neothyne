#ifndef R_LIGHT_HDR
#define R_LIGHT_HDR
#include "r_method.h"
#include "r_gbuffer.h"

namespace m {
    struct mat4;
    struct perspective;
}

namespace r {

struct fog;

struct baseLight {
    baseLight();
    m::vec3 color;
    float ambient;
    union {
        float diffuse;
        float intensity;
    };
    bool highlight;
    bool castShadows;
};

inline baseLight::baseLight()
    : ambient(1.0f)
    , highlight(false)
    , castShadows(true)
{
}

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

// a point light
struct pointLight : baseLight {
    pointLight();
    m::vec3 position;
    float radius;
    size_t hash() const;
};

inline pointLight::pointLight()
    : radius(0.0f)
{
}

// a spot light
struct spotLight : pointLight {
    spotLight();
    m::vec3 direction;
    float cutOff;
    size_t hash() const;
};

inline spotLight::spotLight()
    : cutOff(45.0f)
{
}

struct lightMethod : method {
    lightMethod();

    bool init(const char *vs,
              const char *fs,
              const char *description,
              const u::vector<const char *> &defines = u::vector<const char *>());

    enum {
        // First three must have same layout as gBuffer
        kColor  = gBuffer::kColor,
        kNormal = gBuffer::kNormal,
        kDepth  = gBuffer::kDepth,
        kShadowMap,
        kOcclusion
    };

    void setWVP(const m::mat4 &wvp);
    void setInverse(const m::mat4 &inverse);
    void setColorTextureUnit(int unit);
    void setNormalTextureUnit(int unit);
    void setDepthTextureUnit(int unit);
    void setShadowMapTextureUnit(int unit);
    void setOcclusionTextureUnit(int unit);
    void setEyeWorldPos(const m::vec3 &position);
    void setPerspective(const m::perspective &p);

private:
    uniform *m_WVP;
    uniform *m_inverse;

    uniform *m_colorTextureUnit;
    uniform *m_normalTextureUnit;
    uniform *m_depthTextureUnit;
    uniform *m_shadowMapTextureUnit;
    uniform *m_occlusionTextureUnit;

    uniform *m_eyeWorldPosition;

    uniform *m_screenSize;
    uniform *m_screenFrustum;
};

struct directionalLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const directionalLight &light);
    void setFog(const fog &f);

private:
    uniform *m_light0; // { r, g, b, diffuse }
    uniform *m_light1; // { dir.x, dir.y, dir.z, ambient }
    uniform *m_fog0; // { r, g, b }
    uniform *m_fog1; // { range.x, range.y, density }
    uniform *m_fogEquation;
};

struct pointLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const pointLight &light);
    void setLightWVP(const m::mat4 &wvp);

private:
    uniform *m_light0; // { r, g, b, diffuse }
    uniform *m_light1; // { pos.x, pos.y, pos.z, radius }
    uniform *m_lightWVP;
};

struct spotLightMethod : lightMethod {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setLight(const spotLight &light);
    void setLightWVP(const m::mat4 &wvp);

private:
    uniform *m_light0; // { r, g, b, diffuse }
    uniform *m_light1; // { pos.x, pos.y, pos.z, radius }
    uniform *m_light2; // { dir.x, dir.y, dir.z, cutoff }
    uniform *m_lightWVP;
};

}

#endif
