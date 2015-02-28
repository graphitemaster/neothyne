#ifndef R_WORLD_HDR
#define R_WORLD_HDR
#include "kdmap.h"

#include "r_ssao.h"
#include "r_skybox.h"
#include "r_billboard.h"
#include "r_particles.h"
#include "r_geom.h"
#include "r_model.h"
#include "r_light.h"
#include "r_hoq.h"

#include "u_map.h"

struct world;

namespace r {

struct bboxMethod : method {
    bboxMethod();

    bool init();

    void setWVP(const m::mat4 &wvp);
    void setColor(const m::vec3 &color);

private:
    GLint m_WVPLocation;
    GLint m_colorLocation;
};

inline bboxMethod::bboxMethod()
    : m_WVPLocation(-1)
    , m_colorLocation(-1)
{
}

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
{
}

struct finalMethod : method {
    finalMethod();

    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_screenSizeLocation;
};

inline finalMethod::finalMethod()
    : m_WVPLocation(-1)
    , m_colorMapLocation(-1)
    , m_screenSizeLocation(-1)
{
}

struct finalComposite {
    finalComposite();
    ~finalComposite();

    bool init(const m::perspective &p, GLuint depth);
    void update(const m::perspective &p, GLuint depth);
    void bindWriting();

    GLuint texture() const;

private:
    enum {
        kBuffer,
        kDepth
    };

    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

struct renderTextureBatch {
    int permute;
    size_t start;
    size_t count;
    size_t index;
    material mat; // Rendering material (world and models share this)
};

struct world : geom {
    world();
    ~world();

    bool load(const kdMap &map);
    bool upload(const m::perspective &p);

    void unload(bool destroy = true);
    void render(const pipeline &pl, ::world *map);

private:
    void occlusionPass(const pipeline &pl, ::world *map);
    void geometryPass(const pipeline &pl, ::world *map);
    void lightingPass(const pipeline &pl, ::world *map);
    void forwardPass(const pipeline &pl, ::world *map);
    void compositePass(const pipeline &pl);

    void pointLightPass(const pipeline &pl, const ::world *const map);
    void spotLightPass(const pipeline &pl, const ::world *const map);

    // world shading methods and permutations
    u::vector<geomMethod> m_geomMethods;
    u::vector<finalMethod> m_finalMethods;
    u::vector<directionalLightMethod> m_directionalLightMethods;
    pointLightMethod m_pointLightMethod;
    spotLightMethod m_spotLightMethod;
    ssaoMethod m_ssaoMethod;
    bboxMethod m_bboxMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    sphere m_sphere;
    bbox m_bbox;
    u::map<u::string, model*> m_models;
    u::map<u::string, billboard*> m_billboards;

    // HACK: Testing only
    particleSystem m_particles;

    // HACK: Testing only
    model m_gun;

    // The world itself
    u::vector<uint32_t> m_indices;
    u::vector<kdBinVertex> m_vertices;
    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    gBuffer m_gBuffer;
    ssao m_ssao;
    finalComposite m_final;

    m::mat4 m_identity;
    m::frustum m_frustum;
    occlusionQueries m_queries;

    bool m_uploaded;
};

}

#endif
