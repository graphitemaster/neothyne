#include "engine.h"
#include "world.h"
#include "c_variable.h"

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
    kBillboardJumpPad,
    kBillboardLight,
    kBillboardPlayerStart,
    kBillboardTeleport,
    kBillboardCount
};

void world::unload(bool destroy) {
    // game entities
    for (auto &it : m_mapModels)    delete it;
    for (auto &it : m_playerStarts) delete it;
    for (auto &it : m_jumppads)     delete it;

    // rendering state
    for (auto &it : m_billboards)   delete it;
    for (auto &it : m_pointLights)  delete it;
    for (auto &it : m_spotLights)   delete it;
    for (auto &it : m_models)       delete it.second;
    for (auto &it : m_textures)     delete it.second;

    // release memory backing the containers
    if (destroy) {
        m_map.unload();

        // auxiliaries
        m_entities.destroy();
        m_mapModels.destroy();

        // rendering entities
        m_billboards.destroy();
        m_pointLights.destroy();
        m_spotLights.destroy();

        // game entities
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
    // light billboard
    m_billboards.resize(1);
    m_billboards[0] = new r::billboard;
    if (!m_billboards[0]->load("textures/icons/light"))
        return false;
    return true;
}

bool world::isLoaded() const {
    return m_map.isLoaded();
}

bool world::load(const u::string &file) {
    if (!m_renderer) return false;
    auto read = u::read(neoGamePath() + "maps/" + file, "rb");
    return read && load(*read) && m_renderer->load(&m_map);
}

bool world::upload(const m::perspective &p) {
    if (!m_renderer) return false;
    return m_renderer->upload(p);
}

void world::render(const r::pipeline &pl) {
    if (!m_renderer) return;
    // a new frame reset world state!
    m_renderer->reset();

    float R = ((map_dlight_color >> 16) & 0xFF) / 255.0f;
    float G = ((map_dlight_color >> 8) & 0xFF) / 255.0f;
    float B = (map_dlight_color & 0xFF) / 255.0f;

    r::directionalLight *directionalLight = m_renderer->getDirectionalLight();
    directionalLight->ambient = map_dlight_ambient;
    directionalLight->diffuse = map_dlight_diffuse;
    directionalLight->color = { R, G, B };
    directionalLight->direction = {
        map_dlight_directionx,
        map_dlight_directiony,
        map_dlight_directionz
    };

    R = ((map_fog_color >> 16) & 0xFF) / 255.0f;
    G = ((map_fog_color >> 8) & 0xFF) / 255.0f;
    B = (map_fog_color & 0xFF) / 255.0f;

    r::fog &fog = m_renderer->getFog();
    fog.color = { R, G, B };
    fog.density = map_fog_density;
    fog.start = map_fog_range_start;
    fog.end = map_fog_range_end;
    fog.equation = map_fog_equation;

    // add all light positions into the billboard
    for (auto *it : m_spotLights)
        m_billboards[0]->add(it->position + m::vec3(0.0f, 5.0f, 0.0f));
    for (auto *it : m_pointLights)
        m_billboards[0]->add(it->position + m::vec3(0.0f, 5.0f, 0.0f));

    // add all lights to the renderer (will ignore adding ones which already exist)
    for (auto *it : m_pointLights)
        m_renderer->addPointLight(it);
    for (auto *it : m_spotLights)
        m_renderer->addSpotLight(it);

    // add all billboards
    for (auto *it : m_billboards)
        m_renderer->addBillboard(it);

    // walk the map models and load new ones on demand (renderer will upload them)
    for (const auto &it : m_mapModels) {
        auto find = m_models.find(it->name);
        if (find == m_models.end()) {
            // a new model
            r::model *model = new r::model;
            if (!model->load(m_textures, it->name))
                neoFatal("Failed to load model %s", it->name);
            m_models.insert({ it->name, model });
            m_renderer->addModel(model, it->highlight, it->position, it->scale, it->rotate);
        } else {
            m_renderer->addModel(find->second, it->highlight, it->position, it->scale, it->rotate);
        }
    }

    m_renderer->render(pl);
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

world::descriptor *world::insert(const r::pointLight &it) {
    const size_t index = m_pointLights.size();
    m_pointLights.push_back(copy(it));
    m_entities.push_back({ entity::kPointLight, index, m_entities.size() });
    return &m_entities.back();
}

world::descriptor *world::insert(const r::spotLight &it) {
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
        delete m_mapModels[index];
        m_mapModels.erase(m_mapModels.begin() + index);
        break;
    case entity::kPlayerStart:
        delete m_playerStarts[index];
        m_playerStarts.erase(m_playerStarts.begin() + index);
        break;
    case entity::kPointLight:
        delete m_pointLights[index];
        m_pointLights.erase(m_pointLights.begin() + index);
        break;
    case entity::kSpotLight:
        delete m_spotLights[index];
        m_spotLights.erase(m_spotLights.begin() + index);
        break;
    case entity::kTeleport:
        delete m_teleports[index];
        m_teleports.erase(m_teleports.begin() + index);
        break;
    case entity::kJumppad:
        delete m_jumppads[index];
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

r::directionalLight *world::getDirectionalLight() {
    return m_renderer ? m_renderer->getDirectionalLight() : nullptr;
}

r::spotLight &world::getSpotLight(size_t index) {
    return *m_spotLights[index];
}

r::pointLight &world::getPointLight(size_t index) {
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

ColorGrader *world::getColorGrader() {
    return m_renderer ? m_renderer->getColorGrader() : nullptr;
}
