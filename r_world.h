#ifndef R_WORLD_HDR
#define R_WORLD_HDR
#include "kdmap.h"

#include "r_aa.h"
#include "r_ssao.h"
#include "r_skybox.h"
#include "r_billboard.h"
#include "r_particles.h"
#include "r_geom.h"
#include "r_grader.h"
#include "r_model.h"
#include "r_light.h"
#include "r_hoq.h"
#include "r_shadow.h"

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

struct compositeMethod : method {
    compositeMethod();
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());
    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setColorGradingTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_colorGradingMapLocation;
    GLint m_screenSizeLocation;
};

inline compositeMethod::compositeMethod()
    : m_WVPLocation(-1)
    , m_colorMapLocation(-1)
    , m_colorGradingMapLocation(-1)
    , m_screenSizeLocation(-1)
{
}

struct composite {
    composite();
    ~composite();

    bool init(const m::perspective &p, GLuint depth);
    void update(const m::perspective &p);
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
    bool upload(const m::perspective &p, ::world *map);

    void unload(bool destroy = true);
    void render(const pipeline &pl, ::world *map);

private:
    void cullPass(const pipeline &pl, ::world *map);
    void occlusionPass(const pipeline &pl, ::world *map);
    void shadowPass(const pipeline &pl);
    void geometryPass(const pipeline &pl, ::world *map);
    void lightingPass(const pipeline &pl, ::world *map);
    void forwardPass(const pipeline &pl, ::world *map);
    void compositePass(const pipeline &pl, ::world *map);

    void pointLightPass(const pipeline &pl);
    void spotLightPass(const pipeline &pl);

    // world shading methods and permutations
    geomMethods *m_geomMethods;
    u::vector<directionalLightMethod> m_directionalLightMethods;
    compositeMethod m_compositeMethod;
    pointLightMethod m_pointLightMethod;
    spotLightMethod m_spotLightMethods[2]; // no shadow, shadow
    ssaoMethod m_ssaoMethod;
    bboxMethod m_bboxMethod;
    aaMethod m_aaMethod;
    defaultMethod m_defaultMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    sphere m_sphere;
    bbox m_bbox;
    u::map<u::string, model*> m_models;
    u::map<u::string, billboard*> m_billboards;

    u::vector<r::particleSystem*> m_particleSystems;

    // HACK: Testing only
    model m_gun;

    // The world itself
    u::vector<uint32_t> m_indices;
    u::vector<kdBinVertex> m_vertices;
    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    aa m_aa;
    gBuffer m_gBuffer;
    ssao m_ssao;
    composite m_final;
    grader m_colorGrader;

    m::mat4 m_identity;
    m::frustum m_frustum;
    occlusionQueries m_queries;

    u::vector<spotLight*> m_culledSpotLights;
    u::vector<pointLight*> m_culledPointLights;

    // same order as m_culledSpotLights
    u::vector<shadowMap> m_spotLightShadowMaps;

    shadowMapMethod m_shadowMapMethod;

    bool m_uploaded;
};

}

#endif
