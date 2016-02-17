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
#include "r_shadow.h"
#include "r_composite.h"
#include "r_vignette.h"

#include "u_map.h"

struct world;

namespace r {

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
    void geometryPass(const pipeline &pl);
    void lightingPass(const pipeline &pl);
    void forwardPass(const pipeline &pl);
    void compositePass(const pipeline &pl);

    struct lightChunk {
        lightChunk();
        ~lightChunk();
        size_t hash;
        size_t count;
        bool visible;
        m::mat4 transform;
        GLuint ebo;
        bool init();
    };

    struct spotLightChunk : lightChunk {
        spotLightChunk();
        spotLightChunk(spotLight *light);
        bool buildMesh(kdMap *map);
        spotLight *light;
    };

    struct pointLightChunk : lightChunk {
        pointLightChunk();
        pointLightChunk(pointLight *light);
        bool buildMesh(kdMap *map);
        size_t sideCounts[6];
        pointLight *light;
    };

    void pointLightPass(const pipeline &pl);
    void pointLightShadowPass(const pointLightChunk *const pl);
    void spotLightPass(const pipeline &pl);
    void spotLightShadowPass(const spotLightChunk *const sl);
    void directionalLightPass(const pipeline &pl, bool stencil);

    // world shading methods and permutations
    geomMethods *m_geomMethods;
    u::vector<directionalLightMethod> m_directionalLightMethods;
    pointLightMethod m_pointLightMethods[2]; // no shadow, shadow
    spotLightMethod m_spotLightMethods[2]; // no shadow, shadow

    // Rendering techniques for the world
    method *m_compositeMethod;
    method *m_aaMethod;
    method *m_bboxMethod;
    method *m_screenSpaceMethod;
    method *m_vignetteMethod;
    method *m_ssaoMethod;
    method *m_shadowMapMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    sphere m_sphere;
    bbox m_bbox;
    cone m_cone;
    u::map<u::string, model*> m_models;
    u::map<u::string, billboard*> m_billboards;

    u::vector<r::particleSystem*> m_particleSystems;

    // HACK: Testing only
    model m_gun;

    // The world itself
    ::world *m_map;
    kdMap *m_kdWorld;
    u::vector<uint32_t> m_indices;
    size_t m_memory;

    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    aa m_aa;
    gBuffer m_gBuffer;
    ssao m_ssao;
    composite m_final;
    vignette m_vignette;
    grader m_colorGrader;

    m::mat4 m_identity;
    m::frustum m_frustum;

    u::vector<spotLightChunk> m_culledSpotLights;
    u::vector<pointLightChunk> m_culledPointLights;

    shadowMap m_shadowMap;

    bool m_uploaded;
};

}

#endif
