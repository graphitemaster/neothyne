#include <assert.h>

#include "engine.h"
#include "world.h"
#include "cvar.h"

#include "u_file.h"

NVAR(int, map_dlight_color, "map directional light color", 0, 0x00FFFFFF, 0x00CCCCCC);
NVAR(float, map_dlight_ambient, "map directional light ambient term", 0.0f, 1.0f, 0.50f);
NVAR(float, map_dlight_diffuse, "map directional light diffuse term", 0.0f, 1.0f, 0.75f);
NVAR(float, map_dlight_directionx, "map directional light direction", -1.0f, 1.0f, 1.0f);
NVAR(float, map_dlight_directiony, "map directional light direction", -1.0f, 1.0f, 1.0f);
NVAR(float, map_dlight_directionz, "map directional light direction", -1.0f, 1.0f, 1.0f);

NVAR(float, map_fog_density, "map fog density", 0.0f, 1.0f, 0.5f);
NVAR(int, map_fog_color, "map fog color", 0, 0x00FFFFFF, 0x00CCCCCC);
NVAR(int, map_fog_equation, "map fog equation", 0, 2, 0);
NVAR(float, map_fog_range_start, "map fog range start (for linear only)", 0.0f, 1.0f, 0.0f);
NVAR(float, map_fog_range_end, "map fog range end (for linear only)", 0.0f, 1.0f, 1.0f);

enum {
    kFogLinear,
    kFogExp,
    kFogExp2
};

enum {
    kBillboardJumpPad,
    kBillboardLight,
    kBillboardPlayerStart,
    kBillboardTeleport,
    kBillboardCount
};

world::world()
    : m_directionalLight(nullptr)
{
}

void world::unload(bool destroy) {
    delete m_directionalLight;
    for (auto &it : m_spotLights)
        delete it;
    for (auto &it : m_pointLights)
        delete it;
    for (auto &it : m_mapModels)
        delete it;
    for (auto &it : m_playerStarts)
        delete it;

    m_map.unload();
    m_renderer.unload();

    m_directionalLight = nullptr;

    if (destroy) {
        m_entities.destroy();
        m_billboards.destroy();
        m_spotLights.destroy();
        m_pointLights.destroy();
        m_mapModels.destroy();
        m_playerStarts.destroy();
        m_teleports.destroy();
        m_jumppads.destroy();
    }
}

world::~world() {
    unload(false);
}

bool world::load(const u::vector<unsigned char> &data) {
    // Unload any loaded ones before loading in the new one
    if (isLoaded())
        unload();
    if (!m_map.load(data))
        return false;
    m_billboards.resize(kBillboardCount);
    m_billboards[kBillboardJumpPad] = { "textures/icons/jumppad", 16.0f, true, { } };
    m_billboards[kBillboardLight] = { "textures/icons/light", 16.0f, false, { } };
    m_billboards[kBillboardPlayerStart] = { "textures/icons/playerstart", 16.0f, true, { } };
    m_billboards[kBillboardTeleport] = { "textures/icons/teleport", 16.0f, true, { } };
    return true;
}

bool world::isLoaded() const {
    return m_map.isLoaded();
}

bool world::load(const u::string &file) {
    auto read = u::read(neoGamePath() + "maps/" + file, "rb");
    return read && load(*read) && m_renderer.load(m_map);
}

bool world::upload(const m::perspective &p) {
    return m_renderer.upload(p, this);
}

void world::render(const r::pipeline &pl) {
    if (m_directionalLight) {
        float R = ((map_dlight_color >> 16) & 0xFF) / 255.0f;
        float G = ((map_dlight_color >> 8) & 0xFF) / 255.0f;
        float B = (map_dlight_color & 0xFF) / 255.0f;

        m_directionalLight->ambient = map_dlight_ambient;
        m_directionalLight->diffuse = map_dlight_diffuse;
        m_directionalLight->color = { R, G, B };
        m_directionalLight->direction = {
            map_dlight_directionx,
            map_dlight_directiony,
            map_dlight_directionz
        };

        R = ((map_fog_color >> 16) & 0xFF) / 255.0f;
        G = ((map_fog_color >> 8) & 0xFF) / 255.0f;
        B = (map_fog_color & 0xFF) / 255.0f;

        m_fog.color = { R, G, B };
        m_fog.density = map_fog_density;
        m_fog.start = map_fog_range_start;
        m_fog.end = map_fog_range_end;
        m_fog.equation = map_fog_equation;
    }

    // Erase and generate the billboards each and every frame
    for (auto &it : m_billboards)
        it.boards.clear();
    for (auto &it : m_entities) {
        if (it.type == entity::kPlayerStart) {
            auto &ent = m_playerStarts[it.index];
            m_billboards[kBillboardPlayerStart].add(ent->position, {0.0f, 8.0f, 0.0f}, ent->highlight);
        } else if (it.type == entity::kJumppad) {
            auto &ent = m_jumppads[it.index];
            m_billboards[kBillboardJumpPad].add(ent->position, {0.0f, 8.0f, 0.0f}, ent->highlight);
        } else if (it.type == entity::kTeleport) {
            auto &ent = m_teleports[it.index];
            m_billboards[kBillboardTeleport].add(ent->position, {0.0f, 8.0f, 0.0f}, ent->highlight);
        } else if (it.type == entity::kPointLight) {
            m_billboards[kBillboardLight].add(m_pointLights[it.index]->position, {0.0f, 16.0f, 0.0f}, false);
        } else if (it.type == entity::kSpotLight) {
            m_billboards[kBillboardLight].add(m_spotLights[it.index]->position, {0.0f, 16.0f, 0.0f}, false);
        }
    }

    m_renderer.render(pl, this);
}

void world::setFog(const fog &f) {
    m_fog = f;
}

bool world::trace(const world::trace::query &q, world::trace::hit *h, float maxDistance, bool entities, descriptor *ignore) {
    float min = kMaxTraceDistance;
    m::vec3 position;
    descriptor *ent = nullptr;

    // Note: The following tests all entites in the world.
    // Todo: Use a BIH
    if (entities) {
        for (auto &it : m_entities) {
            // Get position and radius of entity
            float radius = 0.0f;
            if (ignore && (ignore->type == it.type && ignore->index == it.index))
                continue;

            switch (it.type) {
                case entity::kMapModel:
                    position = m_mapModels[it.index]->position;
                    radius = 10.0f; // TODO: calculate sphere radius from bounding box
                    break;
                case entity::kPointLight:
                    position = m_pointLights[it.index]->position;
                    radius = m_pointLights[it.index]->radius;
                    break;
                case entity::kSpotLight:
                    position = m_spotLights[it.index]->position;
                    radius = m_spotLights[it.index]->radius;
                    break;
                case entity::kJumppad:
                    position = m_jumppads[it.index]->position;
                    radius = 8.0f;
                    break;
                case entity::kTeleport:
                    position = m_teleports[it.index]->position;
                    radius = 8.0f;
                    break;
                case entity::kPlayerStart:
                    position = m_playerStarts[it.index]->position;
                    radius = 8.0f;
                    break;
                case entity::kDirectionalLight:
                    break;
            }

            // Entity too small or too far away
            if (radius <= 0.0f || (position - q.start).abs() > maxDistance)
                continue;

            float fraction = 0.0f;
            if (!m::vec3::raySphereIntersect(q.start, q.direction, position, radius, &fraction))
                continue;

            if (fraction >= 0.0f && fraction < min) {
                min = fraction;
                ent = &it;
            }
        }
    }

    h->ent = nullptr;
    if (ent) {
        h->position = q.start + min*q.direction;
        h->normal = (h->position - position).normalized();
        h->ent = ent;
        h->fraction = m::clamp(min, 0.0f, 1.0f);
    }

    // Check the level geometry now (assuming one is loaded)
    if (!m_map.isLoaded())
        return ent;

    kdSphereTrace sphereTrace;
    sphereTrace.start = q.start;
    sphereTrace.direction = q.direction * maxDistance;
    sphereTrace.radius = q.radius;
    m_map.traceSphere(&sphereTrace);

    // Update the trace data
    h->fraction = m::clamp(sphereTrace.fraction, 0.0f, 1.0f);
    h->normal = sphereTrace.plane.n;

    // Hit level geometry
    if (h->fraction < 1.0f) {
        const m::vec3 position = sphereTrace.start + sphereTrace.fraction * sphereTrace.direction;
        // Hit an entity earlier
        if (ent) {
            //Only when the object is nearer than the level geometry
            if ((h->position - q.start).abs() < (position - q.start).abs())
                return true;
        }
        h->ent = nullptr;
        h->position = position;
        return true;
    }
    return ent;
}

template <typename T>
static inline T *copy(const T &other) {
    T *o = new T;
    *o = other;
    return o;
}

void world::insert(const directionalLight &it) {
    delete m_directionalLight;
    m_directionalLight = copy(it);
}

world::descriptor *world::insert(const pointLight &it) {
    const size_t index = m_pointLights.size();
    m_pointLights.push_back(copy(it));
    m_entities.push_back({ entity::kPointLight, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const spotLight &it) {
    const size_t index = m_spotLights.size();
    m_spotLights.push_back(copy(it));
    m_entities.push_back({ entity::kSpotLight, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const mapModel &it) {
    const size_t index = m_mapModels.size();
    m_mapModels.push_back(copy(it));
    m_entities.push_back({ entity::kMapModel, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const playerStart &it) {
    const size_t index = m_playerStarts.size();
    m_playerStarts.push_back(copy(it));
    m_entities.push_back({ entity::kPlayerStart, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const teleport &it) {
    const size_t index = m_teleports.size();
    m_teleports.push_back(copy(it));
    m_entities.push_back({ entity::kTeleport, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const jumppad &it) {
    const size_t index = m_jumppads.size();
    m_jumppads.push_back(copy(it));
    m_entities.push_back({ entity::kJumppad, index, m_entities.size() });
    return &m_entities.back();
}

void world::erase(size_t where) {
    auto &it = m_entities[where];
    size_t index = it.index;
    switch (it.type) {
        case entity::kMapModel:
            m_mapModels.erase(m_mapModels.begin() + index);
            break;
        case entity::kPlayerStart:
            m_playerStarts.erase(m_playerStarts.begin() + index);
            break;
        case entity::kPointLight:
            m_pointLights.erase(m_pointLights.begin() + index);
            break;
        case entity::kSpotLight:
            m_spotLights.erase(m_spotLights.begin() + index);
            break;
        case entity::kTeleport:
            m_teleports.erase(m_teleports.begin() + index);
            break;
        case entity::kJumppad:
            m_jumppads.erase(m_jumppads.begin() + index);
            break;
        default:
            return;
    }
    m_entities.erase(m_entities.begin() + where);

    if (m_entities.size() <= where)
        return;

    // Starting from the entity we just removed. Shift the index down
    // for all others into place
    for (size_t i = where; i < m_entities.size(); ++i) {
        m_entities[i].where--;
        if (m_entities[i].type == it.type) {
            m_entities[i].index = m_entities[i].index
                ? m_entities[i].index - 1 : 0;
        }
    }
}

directionalLight &world::getDirectionalLight() {
    return *m_directionalLight;
}

spotLight &world::getSpotLight(size_t index) {
    return *m_spotLights[index];
}

pointLight &world::getPointLight(size_t index) {
    return *m_pointLights[index];
}

mapModel &world::getMapModel(size_t index) {
    return *m_mapModels[index];
}

playerStart &world::getPlayerStart(size_t index) {
    return *m_playerStarts[index];
}

teleport &world::getTeleport(size_t index) {
    return *m_teleports[index];
}

jumppad &world::getJumppad(size_t index) {
    return *m_jumppads[index];
}

const u::vector<mapModel*> &world::getMapModels() const {
    return m_mapModels;
}

colorGrader &world::getColorGrader() {
    return m_colorGrader;
}
