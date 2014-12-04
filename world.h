#ifndef WORLD_HDR
#define WORLD_HDR
#include "r_world.h"

struct baseLight {
    m::vec3 color;
    float ambient;
    union {
        float diffuse;
        float intensity;
    };
};

// a directional light (local ambiance and diffuse)
struct directionalLight : baseLight {
    m::vec3 direction;
};

// a point light
struct pointLight : baseLight {
    pointLight();
    m::vec3 position;
    float radius;
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
};

inline spotLight::spotLight()
    : cutOff(0.0f)
{
}

// a map model
struct mapModel {
    mapModel();
    m::vec3 position;
    m::vec3 scale;
    m::vec3 rotate;
    size_t index; // m_models index
};

inline mapModel::mapModel()
    : index(0)
{
}

// player start
struct playerStart {
    m::vec3 position;
    m::vec3 direction;
};

struct entity {
    entity();
    enum {
        kUnknown,
        kMapModel,
        kPlayerStart,
        kDirectionalLight,
        kPointLight,
        kSpotLight,
    };
    int type; // One of the things in the enum
    union {
        directionalLight asDirectionalLight;
        pointLight asPointLight;
        spotLight asSpotLight;
        mapModel asMapModel;
        playerStart asPlayerStart;
    };

private:
    friend struct world;
    size_t index; // Real index
};

inline entity::entity()
    : type(kUnknown)
    , index(0)
{
}

struct world {
    bool load(const u::string &map);
    bool upload(const m::perspectiveProjection &project);
    void render(const r::rendererPipeline &pipeline);

    struct trace {
        struct hit {
            m::vec3 position; // Position of what was hit
            m::vec3 normal; // Normal of what was hit
            entity *object; // The entity hit, null for level geometry
            float fraction; // normalized [0, 1] fraction of distance made before hit
        };
        struct query {
            m::vec3 start;
            m::vec3 direction;
            float radius;
        };
    };

    bool trace(const trace::query &q, trace::hit *result, float maxDistance);

    size_t addModel(const u::string &file);

    size_t insert(entity &ent); // Insert an entity
    void erase(size_t where); // Erase an entity

private:
    friend struct r::world;

    static constexpr float kMaxTraceDistance = 99999.9f;

    // Load from compressed data
    bool load(const u::vector<unsigned char> &data);

    r::world m_renderer; // World renderer
    u::vector<r::model> m_models; // TODO: free-standing (without renderer)

    kdMap m_map;

    u::vector<entity> m_entities; // World entities

    // The following are populated via insert/erase
    directionalLight m_directionalLight;
    u::vector<spotLight> m_spotLights;
    u::vector<pointLight> m_pointLights;
    u::vector<mapModel> m_mapModels;
    u::vector<playerStart> m_playerStarts;
};

#endif
