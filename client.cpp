#include "client.h"
#include "engine.h"
#include "c_var.h"

VAR(float, cl_mouse_sens, "mouse sensitivity", 0.01f, 1.0f, 0.1f);
VAR(int, cl_mouse_invert, "invert mouse", 0, 1, 0);

static constexpr float kClientMaxVelocity = 120.0f;
static constexpr m::vec3 kClientGravity(0.0f, -98.0f, 0.0f);
static constexpr float kClientRadius = 5.0f; // client sphere radius (ft / 2pi)
static constexpr float kClientSpeed = 80.0f; // cm/s
static constexpr float kClientCrouchSpeed = 30.0f; // cm/s
static constexpr float kClientJumpSpeed = 130.0f; // cm/s
static constexpr float kClientJumpExponent = 0.3f;
static constexpr float kClientStopSpeed = 90.0f; // -cm/s
static constexpr float kClientCrouchHeight = 3.0f; // ft
static constexpr float kClientCrouchTransitionSpeed = 24.0f; // -m/s
static constexpr float kClientViewHeight = 6.0f; // ft

client::client()
    : m_mouseLat(0.0f)
    , m_mouseLon(0.0f)
    , m_viewHeight(kClientViewHeight)
    , m_origin(0.0f, 150.0f, 0.0f)
    , m_isOnGround(false)
    , m_isOnWall(false)
    , m_isCrouching(false)
{
}

enum {
    COLLIDE_WALL = 1 << 0,
    COLLIDE_GROUND = 1 << 1
};

void client::update(const kdMap &map, float dt) {
    kdSphereTrace trace;
    trace.radius = kClientRadius;

    m::vec3 velocity = m_velocity;
    m::vec3 originalVelocity = m_velocity;
    m::vec3 newVelocity;
    velocity.maxLength(kClientMaxVelocity);

    m::vec3 planes[kdMap::kMaxClippingPlanes];
    m::vec3 pos = m_origin;
    float timeLeft = dt;
    size_t numBumps = kdMap::kMaxBumps;
    size_t numPlanes = 0;

    // Never turn against original velocity
    planes[numPlanes++] = velocity.normalized();

    size_t collide = 0;
    size_t bumpCount = 0;
    for (; bumpCount < numBumps; bumpCount++) {
        // Don't bother if we didn't move
        if (velocity == m::vec3::origin)
            break;

        // Begin the movement trace
        trace.start = pos;
        trace.dir = timeLeft * velocity;
        map.traceSphere(&trace);

        float f = m::clamp(trace.fraction, 0.0f, 1.0f);

        // Moved some distance
        if (f > 0.0f) {
            pos += trace.dir * f * kdMap::kFractionScale;
            originalVelocity = velocity;
            numPlanes = 0;
        }

        // Moved the entire distance
        if (f == 1.0f)
            break;

        timeLeft *= (1.0f - f);

        // We assume a high value in Y to be ground
        if (trace.plane.n[1] > 0.7f)
            collide |= COLLIDE_GROUND;

        // If we made it this far it also means we're hitting a wall
        collide |= COLLIDE_WALL;

        if (numPlanes >= kdMap::kMaxClippingPlanes) {
            velocity = m::vec3(0.0f, 0.0f, 0.0f);
            break;
        }

        // If we hit the same plane before, nudge the velocity ever so slightly
        // away from the plane to deal with non-axial plane sticking
        size_t i;
        for (i = 0; i < numPlanes; i++) {
            if ((trace.plane.n * planes[i]) > 0.99f)
                velocity += trace.plane.n;
        }
        // If we didn't make it through the entire plane set, apply the nudged
        // velocity and try again.
        if (i < numPlanes)
            continue;

        // next clipping plane
        planes[numPlanes++] = trace.plane.n;

        // Find a plane that it enters
        for (size_t i = 0; i < numPlanes; i++) {
            // Check to see if the movement even interacts with the plane
            float into = (originalVelocity * planes[i]);
            // Does it interact?
            if (into >= 0.1f)
                continue;

            // Slide along the plane
            kdMap::clipVelocity(originalVelocity, planes[i], newVelocity, kdMap::kOverClip);

            // Check for a second plane
            for (size_t j = 0; j < numPlanes; j++) {
                if (j == i)
                    continue;

                // Don't process unless movement interacts with the plane
                if (newVelocity * planes[j] >= 0.1f)
                    continue;

                // Try clipping the movement to the plane
                kdMap::clipVelocity(newVelocity, planes[j], newVelocity, kdMap::kOverClip);

                // If it goes back to the first clipping plane then then ignore
                // Otherwise we may stick.
                if (newVelocity * planes[i] >= 0.0f)
                    continue;

                // Slide the original velocity along the crease
                m::vec3 dir = (planes[i] ^ planes[j]).normalized();
                float d = dir * originalVelocity;
                newVelocity = dir * d;

                // Check for a third plane
                for (size_t k = 0; k < numPlanes; k++) {
                    if (k == i || k == j)
                        continue;

                    // Don't process unless movement interacts with the plane
                    if (newVelocity * planes[k] >= 0.1f)
                        continue;

                    // Stop dead for three intersections
                    newVelocity = m::vec3::origin;
                }
            }
        }
        velocity = newVelocity;
    }

    if (bumpCount != 0) {
        // Bumped ourselfs because we didn't make it in the first pass.
        // Here we should do proper STEP traces but for now we dampen the result
        // On Y so that STEPs don't actually throw us in the air.
        velocity.y *= 0.25f;
    }

    m_isOnGround = collide & COLLIDE_GROUND;
    m_isOnWall = collide & COLLIDE_WALL;
    if (m_isOnGround) {
        // Prevent oscillations when we're sitting on the ground
        velocity.y = 0.0f;
    } else {
        // Carry through with gravity
        velocity += kClientGravity * dt;
    }

    // Update the new positions
    m_origin = pos;
    m_velocity = velocity;

    // Query the next changes
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
    m::vec3 newDirection;
    m::vec3 jump;
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

void client::inputMouseMove() {
    float invert = cl_mouse_invert ? 1.0f : -1.0f;

    int deltaX = 0;
    int deltaY = 0;
    neoMouseDelta(&deltaX, &deltaY);

    m_mouseLat -= (float)deltaY * cl_mouse_sens * invert;
    m_mouseLat = m::clamp(m_mouseLat, -89.0f, 89.0f);

    m_mouseLon -= (float)deltaX * cl_mouse_sens * invert;
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

const m::quat &client::getRotation() const {
    return m_rotation;
}

m::vec3 client::getPosition() const {
    // adjust for eye height
    return m::vec3(m_origin.x, m_origin.y + m_viewHeight, m_origin.z);
}
