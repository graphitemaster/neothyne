#include "client.h"
#include "world.h"
#include "edit.h"

extern world::descriptor *gSelected;
extern client gClient;
extern world *gWorld;

namespace edit {

static m::vec3 *getEntityPosition() {
    if (!gSelected) return nullptr;
    switch (gSelected->type) {
    case entity::kMapModel:    return &gWorld->getMapModel(gSelected->index).position;
    case entity::kPlayerStart: return &gWorld->getPlayerStart(gSelected->index).position;
    case entity::kPointLight:  return &gWorld->getPointLight(gSelected->index).position;
    case entity::kSpotLight:   return &gWorld->getSpotLight(gSelected->index).position;
    case entity::kTeleport:    return &gWorld->getTeleport(gSelected->index).position;
    case entity::kJumppad:     return &gWorld->getJumppad(gSelected->index).position;
    default:
        return nullptr;
    }
    return nullptr;
}

static bool *getEntityHighlight() {
    if (!gSelected) return nullptr;
    switch (gSelected->type) {
    case entity::kMapModel:    return &gWorld->getMapModel(gSelected->index).highlight;
    case entity::kPlayerStart: return &gWorld->getPlayerStart(gSelected->index).highlight;
    case entity::kPointLight:  return &gWorld->getPointLight(gSelected->index).highlight;
    case entity::kSpotLight:   return &gWorld->getSpotLight(gSelected->index).highlight;
    case entity::kTeleport:    return &gWorld->getTeleport(gSelected->index).highlight;
    case entity::kJumppad:     return &gWorld->getJumppad(gSelected->index).highlight;
    default:
        return nullptr;
    }
    return nullptr;
}

void deselect() {
    if (!gSelected) return;
    bool *highlight = getEntityHighlight();
    if (highlight)
        *highlight = false;
    gSelected = nullptr;
}

void select() {
    deselect();

    world::trace::hit h;
    world::trace::query q;
    q.start = gClient.getPosition();
    q.radius = 0.01f;
    m::vec3 direction;
    gClient.getDirection(&direction, nullptr, nullptr);
    q.direction = direction.normalized();
    if (gWorld->trace(q, &h, 1024.0f) && h.ent) {
        gSelected = h.ent;
        *getEntityHighlight() = true;
    }
}

void move() {
    m::vec3 direction;
    gClient.getDirection(&direction, nullptr, nullptr);

    // Trace to world
    world::trace::hit h;
    world::trace::query q;
    q.start = gClient.getPosition();
    q.radius = 0.01f;
    q.direction = direction.normalized();

    // Don't collide with anything but geometry for this trace
    static constexpr float kMaxTraceDistance = 1024.0f;
    if (gWorld->trace(q, &h, kMaxTraceDistance, false) && h.fraction > 0.01f) {
        direction *= (kMaxTraceDistance * h.fraction);
        m::vec3 *position = getEntityPosition();
        if (position) {
            *position = q.start + direction;
            position->y += 5.0f; // HACK
        }
    }
}

void remove() {
    if (!gSelected) return;
    gWorld->erase(gSelected->where);
    gSelected = nullptr;
}

}
