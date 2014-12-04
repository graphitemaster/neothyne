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
    const float R = ((map_dlight_color >> 16) & 0xFF) / 255.0f;
    const float G = ((map_dlight_color >> 8) & 0xFF) / 255.0f;
    const float B = (map_dlight_color & 0xFF) / 255.0f;

    m_directionalLight.ambient = map_dlight_ambient;
    m_directionalLight.diffuse = map_dlight_diffuse;
    m_directionalLight.color = { R, G, B };
    m_directionalLight.direction = {
        map_dlight_directionx,
        map_dlight_directiony,
        map_dlight_directionz
    };

    m_renderer.render(pipeline, this);
}

bool world::trace(const world::trace::query &q, world::trace::hit *h, float maxDistance) {
    float min = kMaxTraceDistance;
    entity *obj = nullptr;
    m::vec3 position;

    // Note: The following tests all entites in the world.
    // Todo: Use a BIH
    for (auto &it : m_entities) {
        // Get position and radius of entity
        float radius = 0.0f;
        switch (it.type) {
            case entity::kMapModel:
                position = it.asMapModel.position;
                radius = 100.0f; // TODO: calculate sphere radius from bounding box
                break;
        }

        // Entity too small or too far away
        if (radius <= 0.0f || (position - q.start).abs() > maxDistance)
            continue;

        float fraction;
        if (!m::vec3::raySphereIntersect(q.start, q.direction, position, radius, &fraction))
            continue;

        if (fraction >= 0.0f && fraction < min) {
            min = fraction;
            obj = &it;
        }
    }

    if (obj) {
        h->position = q.start + min*q.direction;
        h->normal = (h->position - position).normalized();
        h->object = obj;
        h->fraction = min;
    } else {
        h->object = nullptr;
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
        // Hit an object earlier
        if (h->object) {
            // Only when the object is nearer than the level geometry
            if ((h->position - q.start).abs() < (position - q.start).abs())
                return true;
        }
        h->position = position;
        return true;
    }
    return h->object;
}

size_t world::insert(entity &ent) {
    size_t where = m_entities.size();
    switch (ent.type) {
        case entity::kMapModel:
            ent.index = m_mapModels.size();
            m_mapModels.push_back(ent.asMapModel);
            break;
        case entity::kPlayerStart:
            ent.index = m_playerStarts.size();
            m_playerStarts.push_back(ent.asPlayerStart);
            break;
        case entity::kPointLight:
            ent.index = m_pointLights.size();
            m_pointLights.push_back(ent.asPointLight);
            break;
        case entity::kSpotLight:
            ent.index = m_spotLights.size();
            m_spotLights.push_back(ent.asSpotLight);
            break;
        case entity::kDirectionalLight:
            m_directionalLight = ent.asDirectionalLight;
            return entity::kDirectionalLight;
    }
    m_entities.push_back(ent);
    return where;
}

void world::erase(size_t where) {
    auto &it = m_entities[where];

    #define ERASE(WHAT) \
        (WHAT).erase((WHAT).begin() + it.index, (WHAT).begin() + it.index + 1)

    switch (it.type) {
        case entity::kMapModel:
            ERASE(m_mapModels);
            break;
        case entity::kPlayerStart:
            ERASE(m_playerStarts);
            break;
        case entity::kPointLight:
            ERASE(m_pointLights);
            break;
        case entity::kSpotLight:
            ERASE(m_spotLights);
            break;
    }

    m_entities.erase(m_entities.begin() + where, m_entities.begin() + where + 1);
}
