#ifndef R_WORLD_HDR
#define R_WORLD_HDR
#include "kdmap.h"
#include "grader.h"

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
#include "r_pipeline.h"

#include "u_map.h"

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
    bool upload(const m::perspective &p);

    void unload(bool destroy = true);
    void render(const pipeline &pl);

    // called before start of rendering to clear billboards
    // and other things that get sent to the world every frame
    void reset() {
        // walk all the lights and mark them for collection
        for (auto &it : m_models)
            it.second->collect = true;
        for (auto &it : m_culledSpotLights)
            it.second->collect = true;
        for (auto &it : m_culledPointLights)
            it.second->collect = true;
        for (auto &it : m_billboards)
            it.second.first = true;
    }

    // to add raw primitives into the world
    void addBillboard(r::billboard *billboard_);
    void addPointLight(r::pointLight *light);
    void addSpotLight(r::spotLight *light);
    void addModel(r::model *model_,
                  bool highlight,
                  const m::vec3 &position,
                  const m::vec3 &scale,
                  const m::vec3 &rotate);

    // global entities
    fog &getFog();
    directionalLight &getDirectionalLight();
    ColorGrader &getColorGrader();

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
        size_t memory;
        bool collect;
        bool visible;
        m::mat4 transform;
        GLuint ebo;
        r::stat *stats;
        bool init(const char *name, const char *description);
    };

    struct spotLightChunk : lightChunk {
        spotLightChunk();
        spotLightChunk(const r::spotLight *light);
        bool buildMesh(kdMap *map);
        const r::spotLight *light;
    };

    struct pointLightChunk : lightChunk {
        pointLightChunk();
        pointLightChunk(const r::pointLight *light);
        bool buildMesh(kdMap *map);
        size_t sideCounts[6];
        const r::pointLight *light;
    };

    struct modelChunk {
        modelChunk();
        modelChunk(const r::model *model,
                   bool highlight,
                   const m::vec3 &position,
                   const m::vec3 &scale,
                   const m::vec3 &rotate);
        void animate(float frame); // TODO:
        m::vec3 position;
        m::vec3 scale;
        m::vec3 rotate;
        float frame;
        bool collect;
        bool highlight;
        bool visible;
        const r::model *model;
        r::pipeline pipeline;
    };

    void pointLightPass(const pipeline &pl);
    void pointLightShadowPass(const pointLightChunk *const pl);
    void spotLightPass(const pipeline &pl);
    void spotLightShadowPass(const spotLightChunk *const sl);
    void directionalLightPass(const pipeline &pl, bool stencil);

    // represents all six frustum planes used for frustum culling
    // spheres, points and bounding boxes
    m::frustum m_frustum;

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
    vignetteMethod m_vignetteMethod;
    shadowMapMethod m_shadowMapMethod;

    // Other things in the world to render
    skybox m_skybox;
    quad m_quad;
    sphere m_sphere;
    bbox m_bbox;
    cone m_cone;

    // global entities
    directionalLight m_directionalLight;
    fog m_fog;
    ColorGrader m_grader;

    u::vector<r::particleSystem*> m_particleSystems;
    // HACK: Testing only
    model m_gun;

    // The world itself
    kdMap *m_kdWorld;
    u::vector<uint32_t> m_indices;

    // TODO: cleanup
    u::vector<renderTextureBatch> m_textureBatches;
    u::map<u::string, texture2D*> m_textures2D;

    // render buffers
    aa m_aa;
    gBuffer m_gBuffer;
    ssao m_ssao;
    composite m_final;
    vignette m_vignette;
    grader m_colorGrader;
    shadowMap m_shadowMap;

    u::map<r::model*, modelChunk*> m_models;
    u::map<r::spotLight*, spotLightChunk*> m_culledSpotLights;
    u::map<r::pointLight*, pointLightChunk*> m_culledPointLights;
    u::map<r::billboard*, u::pair<bool, r::billboard*>> m_billboards;

    bool m_uploaded;

    r::stat *m_stats;
};

}

#endif
