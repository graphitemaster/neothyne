#include "client.h"
#include "engine.h"

static constexpr float kClientMaxVelocity = 120.0f;
static constexpr m::vec3 kClientGravity(0.0f, -98.0f, 0.0f);
static constexpr float kClientRadius = 5.0f; // client sphere radius (ft / 2pi)
static constexpr float kClientWallReduce = 0.5f; // 50% reduction of collision responce when sliding up a wal
static constexpr float kClientSpeed = 80.0f; // cm/s
static constexpr float kClientCrouchSpeed = 30.0f; // cm/s
static constexpr float kClientJumpSpeed = 130.0f; // cm/s
static constexpr float kClientJumpExponent = 0.3f;
static constexpr float kClientStopSpeed = 90.0f; // -cm/s
static constexpr float kClientCrouchHeight = 3.0f; // ft
static constexpr float kClientCrouchTransitionSpeed = 24.0f; // -m/s
static constexpr float kClientViewHeight = 6.0f; // ft

client::client() :
    m_mouseLat(0.0f),
    m_mouseLon(0.0f),
    m_viewHeight(kClientViewHeight),
    m_origin(0.0f, 150.0f, 0.0f),
    m_velocity(0.0f, 0.0f, 0.0f),
    m_isOnGround(false),
    m_isOnWall(false),
    m_isCrouching(false)
{

}

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
    kdSphereTrace trace;
    trace.radius = kClientRadius;

    m::vec3 velocity = m_velocity;
    m::vec3 originalVelocity = m_velocity;
    m::vec3 primalVelocity = m_velocity;
    m::vec3 newVelocity;
    velocity.maxLength(kClientMaxVelocity);

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

        timeLeft *= (1.0f - f);

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

    if (!allFraction) {
        velocity = m::vec3(0.0f, 0.0f, 0.0f);
    }

    m_isOnGround = groundHit;
    m_isOnWall = wallHit;
    if (groundHit) {
        velocity.y = 0.0f;
    } else {
        velocity += kClientGravity * dt;
        if (wallHit)
            velocity.y *= kClientWallReduce;
    }

    // we could be stuck
    if (map.isSphereStuck(pos, kClientRadius)) {
        m_origin = pos;
        tryUnstick(map, kClientRadius);
    }

    m_origin = pos;
    m_velocity = velocity;

    u::vector<clientCommands> commands;
    inputGetCommands(commands);
    inputMouseMove();

    move(dt, commands);
}

void client::move(float dt, const u::vector<clientCommands> &commands) {
    m::vec3 velocity = m_velocity;
    m::vec3 direction;
    m::vec3 side;
    m::vec3 up;
    m::vec3 newDirection(0.0f, 0.0f, 0.0f);
    m::vec3 jump(0.0f, 0.0f, 0.0f);
    m::quat rotation = m_rotation;
    bool needSlowDown = true;

    rotation.getOrient(&direction, &up, &side);

    // at half of the 45 degrees in either direction invert the sign
    // We do it between two points to prevent a situation where the
    // camera is just at the right axis thus preventing movement.
    up = (m::toDegree(direction.y) > 45.0f / 2.0f) ? -up : up;
    m::vec3 upCamera;
    bool crouchReleased = true;
    for (auto &it : commands) {
        switch (it) {
            case kCommandForward:
                newDirection += direction + up;
                needSlowDown = false;
                break;
            case kCommandBackward:
                newDirection -= direction + up;
                needSlowDown = false;
                break;
            case kCommandLeft:
                newDirection -= side;
                needSlowDown = false;
                break;
            case kCommandRight:
                newDirection += side;
                needSlowDown = false;
                break;
            case kCommandJump:
                jump = m::vec3(0.0f, 8.0f, 0.0f);
                break;
            case kCommandCrouch:
                crouchReleased = false;
                if (m_isOnGround)
                    m_isCrouching = true;
                break;
        }
    }

    if (crouchReleased)
        m_isCrouching = false;

    float clientSpeed;
    float crouchTransitionSpeed = kClientCrouchTransitionSpeed * dt;
    if (m_isCrouching) {
        clientSpeed = kClientCrouchSpeed;
        m_viewHeight = m::clamp(m_viewHeight - crouchTransitionSpeed,
            kClientCrouchHeight, m_viewHeight - crouchTransitionSpeed);
    } else {
        clientSpeed = kClientSpeed;
        if (m_viewHeight < kClientViewHeight)
            m_viewHeight = m::clamp(m_viewHeight + crouchTransitionSpeed,
                m_viewHeight + crouchTransitionSpeed, kClientViewHeight);
    }

    newDirection.y = 0.0f;
    if (newDirection.absSquared() > 0.1f)
        newDirection.setLength(clientSpeed);
    newDirection.y += velocity.y;
    if (m_isOnGround)
        newDirection += jump * powf(kClientJumpSpeed, kClientJumpExponent);
    if (needSlowDown) {
        m::vec3 slowDown = m_velocity * kClientStopSpeed * ((1.0f - dt) * 0.01f);
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

    int deltaX = 0;
    int deltaY = 0;
    neoMouseDelta(&deltaX, &deltaY);

    m_mouseLat -= (float)deltaY * kSensitivity * invert;
    m_mouseLat = m::clamp(m_mouseLat, -89.0f, 89.0f);

    m_mouseLon -= (float)deltaX * kSensitivity * invert;
    m_mouseLon = m::angleMod(m_mouseLon);

    m::quat qlat(m::vec3::xAxis, m::toRadian(m_mouseLat));
    m::quat qlon(m::vec3::yAxis, m::toRadian(m_mouseLon));

    setRotation(qlon * qlat);
}

void client::inputGetCommands(u::vector<clientCommands> &commands) {
    u::map<int, int> &keyState = neoKeyState();
    commands.clear();
    if (keyState[SDLK_w])      commands.push_back(kCommandForward);
    if (keyState[SDLK_s])      commands.push_back(kCommandBackward);
    if (keyState[SDLK_a])      commands.push_back(kCommandLeft);
    if (keyState[SDLK_d])      commands.push_back(kCommandRight);
    if (keyState[SDLK_SPACE])  commands.push_back(kCommandJump);
    if (keyState[SDLK_LSHIFT]) commands.push_back(kCommandCrouch);
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
    return m::vec3(m_origin.x, m_origin.y + m_viewHeight, m_origin.z);
}
