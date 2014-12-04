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
    u::string name;
    bool highlight;
};

inline mapModel::mapModel()
    : highlight(false)
{
}

// player start
struct playerStart {
    m::vec3 position;
    m::vec3 direction;
};

enum class entity {
    kUnknown,
    kMapModel,
    kPlayerStart,
    kDirectionalLight,
    kPointLight,
    kSpotLight,
};

struct world {
    world();
    ~world();

    bool load(const u::string &map);
    bool upload(const m::perspectiveProjection &project);
    void render(const r::rendererPipeline &pipeline);

    // World entity descriptor
    struct descriptor {
        entity type;
        size_t index;
    };

    struct trace {
        struct hit {
            m::vec3 position; // Position of what was hit
            m::vec3 normal; // Normal of what was hit
            descriptor *ent; // The entity hit or null if level geometry
            float fraction; // normalized [0, 1] fraction of distance made before hit
        };
        struct query {
            m::vec3 start;
            m::vec3 direction;
            float radius;
        };
    };

    bool trace(const trace::query &q, trace::hit *result, float maxDistance);

    void insert(const directionalLight &it);
    size_t insert(const spotLight &it);
    size_t insert(const pointLight &it);
    size_t insert(const mapModel &it);
    size_t insert(const playerStart &it);

    void erase(size_t ent); // Erase an entity

    directionalLight &getDirectionalLight();
    spotLight &getSpotLight(size_t index);
    pointLight &getPointLight(size_t index);
    mapModel &getMapModel(size_t index);
    playerStart &getPlayerStart(size_t index);

protected:
    friend struct r::world;

    static constexpr float kMaxTraceDistance = 99999.9f;

    // Load from compressed data
    bool load(const u::vector<unsigned char> &data);

private:
    kdMap m_map; // The map for this world

    r::world m_renderer;

    u::vector<descriptor> m_entities;

    // The following are populated via insert/erase
    directionalLight *m_directionalLight;
    u::vector<spotLight*> m_spotLights;
    u::vector<pointLight*> m_pointLights;
    u::vector<mapModel*> m_mapModels;
    u::vector<playerStart*> m_playerStarts;
};

#endif
