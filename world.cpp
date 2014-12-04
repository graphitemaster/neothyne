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

world::world()
    : m_directionalLight(nullptr)
{
}

world::~world() {
    delete m_directionalLight;
    for (auto &it : m_spotLights)
        delete it;
    for (auto &it : m_pointLights)
        delete it;
    for (auto &it : m_mapModels)
        delete it;
    for (auto &it : m_playerStarts)
        delete it;
}

bool world::load(const u::vector<unsigned char> &data) {
    return m_map.load(data);
}

bool world::load(const u::string &file) {
    auto read = u::read(neoGamePath() + "maps/" + file, "rb");
    return read && load(*read) && m_renderer.load(m_map);
}

bool world::upload(const m::perspectiveProjection &project) {
    return m_renderer.upload(project);
}

void world::render(const r::rendererPipeline &pipeline) {
    if (m_directionalLight) {
        const float R = ((map_dlight_color >> 16) & 0xFF) / 255.0f;
        const float G = ((map_dlight_color >> 8) & 0xFF) / 255.0f;
        const float B = (map_dlight_color & 0xFF) / 255.0f;

        m_directionalLight->ambient = map_dlight_ambient;
        m_directionalLight->diffuse = map_dlight_diffuse;
        m_directionalLight->color = { R, G, B };
        m_directionalLight->direction = {
            map_dlight_directionx,
            map_dlight_directiony,
            map_dlight_directionz
        };
    }
    m_renderer.render(pipeline, this);
}

bool world::trace(const world::trace::query &q, world::trace::hit *h, float maxDistance, descriptor *ignore) {
    float min = kMaxTraceDistance;
    m::vec3 position;
    descriptor *ent = nullptr;

    // Note: The following tests all entites in the world.
    // Todo: Use a BIH
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
            default:
                break;
            //case entity::kPointLight:
            //    position = it.asPointLight.position;
            //    radius = it.asPointLight.radius;
            //    break;
            //case entity::kSpotLight:
            //    position = it.asSpotLight.position;
            //    radius = it.asSpotLight.radius;
            //    break;
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

    if (ent) {
        h->position = q.start + min*q.direction;
        h->normal = (h->position - position).normalized();
        h->ent = ent;
        h->fraction = m::clamp(min, 0.0f, 1.0f);
        return true;
    } else {
        h->ent = nullptr;
    }

    // Check the level geometry now
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
            // Only when the object is nearer than the level geometry
            if ((h->position - q.start).abs() < (position - q.start).abs())
                return true;
        }
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

size_t world::insert(const pointLight &it) {
    size_t index = m_pointLights.size();
    m_pointLights.push_back(copy(it));
    const size_t size = m_entities.size();
    m_entities.push_back({ entity::kPointLight, index });
    return size;
}

size_t world::insert(const spotLight &it) {
    size_t index = m_spotLights.size();
    m_spotLights.push_back(copy(it));
    const size_t size = m_entities.size();
    m_entities.push_back({ entity::kSpotLight, index });
    return size;
}

size_t world::insert(const mapModel &it) {
    size_t index = m_mapModels.size();
    m_mapModels.push_back(copy(it));
    const size_t size = m_entities.size();
    m_entities.push_back({ entity::kMapModel, index });
    return size;
}

size_t world::insert(const playerStart &it) {
    size_t index = m_playerStarts.size();
    m_playerStarts.push_back(copy(it));
    const size_t size = m_entities.size();
    m_entities.push_back({ entity::kPlayerStart, index });
    return size;
}

void world::erase(size_t where) {
    auto &it = m_entities[where];
    switch (it.type) {
        case entity::kMapModel:
            m_mapModels.erase(m_mapModels.begin() + it.index);
            break;
        case entity::kPlayerStart:
            m_playerStarts.erase(m_playerStarts.begin() + it.index);
            break;
        case entity::kPointLight:
            m_pointLights.erase(m_pointLights.begin() + it.index);
            break;
        case entity::kSpotLight:
            m_spotLights.erase(m_spotLights.begin() + it.index);
            break;
        default:
            break;
    }
    m_entities.erase(m_entities.begin() + where);
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
