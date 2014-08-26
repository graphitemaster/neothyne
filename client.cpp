#include <SDL2/SDL.h>

#include "client.h"

client::client() :
    m_mouseLat(0.0f),
    m_mouseLon(0.0f),
    m_origin(0.0f, 150.0f, 0.0f),
    m_velocity(0.0f, 0.0f, 0.0f),
    m_isOnGround(false),
    m_isOnWall(false)
{

}

u::map<int, int> &getKeyState(int key = 0, bool keyDown = false, bool keyUp = false);
void getMouseDelta(int *deltaX, int *deltaY);

bool client::tryUnstick(const kdMap &map, float radius) {
    static const m::vec3 offsets[] = {
        m::vec3(-1.0f, 1.0f,-1.0f),
        m::vec3( 1.0f, 1.0f,-1.0f),
        m::vec3( 1.0f, 1.0f, 1.0f),
        m::vec3(-1.0f, 1.0f, 1.0f),
        m::vec3(-1.0f,-1.0f,-1.0f),
        m::vec3( 1.0f,-1.0f,-1.0f),
        m::vec3( 1.0f,-1.0f, 1.0f),
        m::vec3(-1.0f,-1.0f, 1.0f)
    };
    const float radiusScale = radius * 0.1f;
    for (size_t j = 1; j < 4; j++) {
        for (size_t i = 0; i < sizeof(offsets)/sizeof(offsets[0]); i++) {
            m::vec3 tryPosition = m_origin + j * radiusScale * offsets[i];
            if (map.isSphereStuck(tryPosition, radius))
                continue;
            m_origin = tryPosition;
            return true;
        }
    }
    return false;
}

void client::update(const kdMap &map, float dt) {
    static constexpr float kMaxVelocity = 80.0f;
    static constexpr m::vec3 kGravity(0.0f, -98.0f, 0.0f);
    static constexpr float kRadius = 5.0f;

    kdSphereTrace trace;
    trace.radius = kRadius;

    m::vec3 velocity = m_velocity;
    m::vec3 originalVelocity = m_velocity;
    m::vec3 primalVelocity = m_velocity;
    m::vec3 newVelocity;
    velocity.maxLength(kMaxVelocity);

    m::vec3 planes[kdMap::kMaxClippingPlanes];
    m::vec3 pos = m_origin;
    float timeLeft = dt;
    float allFraction = 0.0f;
    size_t numBumps = kdMap::kMaxBumps;
    size_t numPlanes = 0;

    bool wallHit = false;
    bool groundHit = false;
    for (size_t bumpCount = 0; bumpCount < numBumps; bumpCount++) {
        if (velocity == m::vec3(0.0f, 0.0f, 0.0f))
            break;

        trace.start = pos;
        trace.dir = timeLeft * velocity;
        map.traceSphere(&trace);

        float f = m::clamp(trace.fraction, 0.0f, 1.0f);
        allFraction += f;

        if (f > 0.0f) {
            pos += trace.dir * f * kdMap::kFractionScale;
            originalVelocity = velocity;
            numPlanes = 0;
        }

        if (f == 1.0f)
            break;

        wallHit = true;

        timeLeft = timeLeft * (1.0f - f);

        if (trace.plane.n[1] > 0.7f)
            groundHit = true;

        if (numPlanes >= kdMap::kMaxClippingPlanes) {
            velocity = m::vec3(0.0f, 0.0f, 0.0f);
            break;
        }

        // next clipping plane
        planes[numPlanes++] = trace.plane.n;

        size_t i, j;
        for (i = 0; i < numPlanes; i++) {
            kdMap::clipVelocity(originalVelocity, planes[i], newVelocity, kdMap::kOverClip);
            for (j = 0; j < numPlanes; j++)
                if (j != i && !(planes[i] == planes[j]))
                    if (newVelocity * planes[j] < 0.0f)
                        break;
            if (j == numPlanes)
                break;
        }

        // did we make it through the entire plane set?
        if (i != numPlanes)
            velocity = newVelocity;
        else {
            // go along the planes crease
            if (numPlanes != 2) {
                velocity = m::vec3(0.0f, 0.0f, 0.0f);
                break;
            }
            m::vec3 dir = planes[0] ^ planes[1];
            float d = dir * velocity;
            velocity = dir * d;
        }

        if (velocity * primalVelocity <= 0.0f) {
            velocity = m::vec3(0.0f, 0.0f, 0.0f);
            break;
        }
    }

    if (allFraction == 0.0f) {
        velocity = m::vec3(0.0f, 0.0f, 0.0f);
    }

    m_isOnGround = groundHit;
    m_isOnWall = wallHit;
    if (groundHit) {
        velocity.y = 0.0f;
    } else {
        velocity += kGravity * dt;
    }

    // we could be stuck
    if (map.isSphereStuck(pos, kRadius)) {
        m_origin = pos;
        tryUnstick(map, kRadius);
    }

    m_origin = pos;
    m_velocity = velocity;

    u::vector<clientCommands> commands;
    inputGetCommands(commands);
    inputMouseMove();

    move(commands);
}

void client::move(const u::vector<clientCommands> &commands) {
    m::vec3 velocity = m_velocity;
    m::vec3 direction;
    m::vec3 side;
    m::vec3 up;
    m::vec3 newDirection(0.0f, 0.0f, 0.0f);
    m::vec3 jump(0.0f, 0.0f, 0.0f);
    m::quat rotation = m_rotation;

    rotation.getOrient(&direction, &up, &side);

    m::vec3 upCamera;
    for (auto &it : commands) {
        switch (it) {
            case kCommandForward:
                newDirection += direction + up;
                break;
            case kCommandBackward:
                newDirection -= direction + up;
                break;
            case kCommandLeft:
                newDirection -= side;
                break;
            case kCommandRight:
                newDirection += side;
                break;
            case kCommandJump:
                jump = m::vec3(0.0f, 0.25f, 0.0f);
                break;
        }
    }

    const float kClientSpeed = 60.0f; // cm/s
    const float kJumpSpeed = 130.0f; // cm/s
    const float kStopSpeed = 90.0f; // -cm/s

    newDirection.y = 0.0f;
    if (newDirection.absSquared() > 0.1f)
        newDirection.setLength(kClientSpeed);
    newDirection.y += velocity.y;
    if (m_isOnGround) {
        newDirection += jump * kJumpSpeed;
    }
    if (commands.size() == 0) {
        m::vec3 slowDown = m_velocity * kStopSpeed * 0.01f;
        slowDown.y = 0.0f;
        newDirection += slowDown;
    }
    m_lastDirection = direction;
    m_velocity = newDirection;
}

void client::inputMouseMove(void) {
    static const float kSensitivity = 0.50f / 6.0f;
    static const bool kInvert = true;
    const float invert = kInvert ? -1.0f : 1.0f;

    int deltaX;
    int deltaY;
    getMouseDelta(&deltaX, &deltaY);

    m_mouseLat -= (float)deltaY * kSensitivity * invert;
    m_mouseLat = m::clamp(m_mouseLat, -89.0f, 89.0f);

    m_mouseLon -= (float)deltaX * kSensitivity * invert;
    m_mouseLon = m::angleMod(m_mouseLon);

    m::quat qlat(m::vec3::xAxis, m_mouseLat * m::kDegToRad);
    m::quat qlon(m::vec3::yAxis, m_mouseLon * m::kDegToRad);

    setRotation(qlon * qlat);
}

void client::inputGetCommands(u::vector<clientCommands> &commands) {
    u::map<int, int> &keyState = getKeyState();
    commands.clear();
    if (keyState[SDLK_w])     commands.push_back(kCommandForward);
    if (keyState[SDLK_s])     commands.push_back(kCommandBackward);
    if (keyState[SDLK_a])     commands.push_back(kCommandLeft);
    if (keyState[SDLK_d])     commands.push_back(kCommandRight);
    if (keyState[SDLK_SPACE]) commands.push_back(kCommandJump);
}

void client::setRotation(const m::quat &rotation) {
    m_rotation = rotation;
}

void client::getDirection(m::vec3 *direction, m::vec3 *up, m::vec3 *side) const {
    return m_rotation.getOrient(direction, up, side);
}

const m::quat &client::getRotation(void) const {
    return m_rotation;
}

m::vec3 client::getPosition(void) const {
    // adjust for eye height
    return m::vec3(m_origin.x, m_origin.y + 5.5f, m_origin.z);
}
