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
    uniform *m_WVP;
    uniform *m_color;
};

inline bboxMethod::bboxMethod()
    : m_WVP(nullptr)
    , m_color(nullptr)
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
    uniform *m_WVP;
    uniform *m_colorMap;
    uniform *m_colorGradingMap;
    uniform *m_screenSize;
};

inline compositeMethod::compositeMethod()
    : m_WVP(nullptr)
    , m_colorMap(nullptr)
    , m_colorGradingMap(nullptr)
    , m_screenSize(nullptr)
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

    bool load(kdMap *map);
    bool upload(const m::perspective &p, ::world *map);

    void unload(bool destroy = true);
    void render(const pipeline &pl);

private:
    void cullPass(const pipeline &pl);
    void occlusionPass(const pipeline &pl);
    void geometryPass(const pipeline &pl);
    void lightingPass(const pipeline &pl);
    void forwardPass(const pipeline &pl);
    void compositePass(const pipeline &pl);

    struct lightChunk {
        size_t hash;
        size_t count;
        GLuint ebo;
        bool visible;
    };

    struct spotLightChunk {
        ~spotLightChunk();
        size_t hash;
        size_t count;
        spotLight *light;
        GLuint ebo;
        bool visible;
        bool init();
        bool buildMesh(kdMap *map);
    };

    struct pointLightChunk {
        ~pointLightChunk();
        size_t hash;
        size_t count;
        size_t sideCounts[6];
        pointLight *light;
        GLuint ebo;
        bool visible;
        bool init();
        bool buildMesh(kdMap *map);
    };

    void pointLightPass(const pipeline &pl);
    void pointLightShadowPass(const pointLightChunk *const pl);
    void spotLightPass(const pipeline &pl);
    void spotLightShadowPass(const spotLightChunk *const sl);

    // world shading methods and permutations
    geomMethods *m_geomMethods;
    u::vector<directionalLightMethod> m_directionalLightMethods;
    compositeMethod m_compositeMethod;
    pointLightMethod m_pointLightMethods[2]; // no shadow, shadow
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
    ::world *m_map;
    kdMap *m_kdWorld;
    u::vector<uint32_t> m_indices;

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

    u::vector<spotLightChunk> m_culledSpotLights;
    u::vector<pointLightChunk> m_culledPointLights;

    shadowMap m_shadowMap;
    shadowMapMethod m_shadowMapMethod;

    bool m_uploaded;
};

}

#endif
