#ifndef WORLD_HDR
#define WORLD_HDR
#include "r_light.h"
#include "r_skybox.h"
#include "r_world.h"

// a map model
struct mapModel {
    mapModel();
    m::vec3 position;
    m::vec3 scale;
    m::vec3 rotate;
    u::string name;
    bool highlight;
    float curFrame;
};

inline mapModel::mapModel()
    : highlight(false)
    , curFrame(0.0f)
{
}

struct playerStart {
    m::vec3 position;
    m::vec3 direction;
    bool highlight;
};

struct teleport {
    m::vec3 position;
    m::vec3 direction;
    bool highlight;
};

struct jumppad {
    m::vec3 position;
    m::vec3 direction;
    m::vec3 velocity;
    bool highlight;
};

enum class entity {
    kMapModel,
    kPlayerStart,
    kDirectionalLight,
    kPointLight,
    kSpotLight,
    kTeleport,
    kJumppad
};

struct world {
    ~world();

    bool load(const u::string &map);
    bool upload(const m::perspective &p);
    void render(const r::pipeline &pl);

    void setFog(const r::fog &f);

    void unload(bool destroy = true);
    bool isLoaded() const;

    // World entity descriptor
    struct descriptor {
        entity type;
        size_t index;
        size_t where;
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

    bool trace(const trace::query &q, trace::hit *result, float maxDistance, bool entities = true, descriptor *ignore = nullptr);

    descriptor *insert(const r::spotLight &it);
    descriptor *insert(const r::pointLight &it);
    descriptor *insert(const mapModel &it);
    descriptor *insert(const playerStart &it);
    descriptor *insert(const teleport &it);
    descriptor *insert(const jumppad &it);

    void erase(size_t where); // Erase an entity

    r::directionalLight &getDirectionalLight();
    r::spotLight &getSpotLight(size_t index);
    r::pointLight &getPointLight(size_t index);
    mapModel &getMapModel(size_t index);
    playerStart &getPlayerStart(size_t index);
    teleport &getTeleport(size_t index);
    jumppad &getJumppad(size_t index);
    ColorGrader &getColorGrader();

    const u::vector<mapModel*> &getMapModels() const;

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
    u::vector<r::billboard*> m_billboards;
    u::vector<r::spotLight*> m_spotLights;
    u::vector<r::pointLight*> m_pointLights;

    u::vector<mapModel*> m_mapModels;
    u::vector<playerStart*> m_playerStarts;
    u::vector<teleport*> m_teleports;
    u::vector<jumppad*> m_jumppads;

    // internal rendering state for the world
    u::map<u::string, r::texture2D*> m_textures;
    u::map<u::string, r::model*> m_models;
};

#endif
