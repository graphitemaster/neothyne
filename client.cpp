#include <SDL2/SDL.h>

#include "client.h"

client::client() :
    m_mouseLat(0.0f),
    m_mouseLon(0.0f),
    m_origin(0.0f, 150.0f, 0.0f),
    m_velocity(0.0f, 0.0f, 0.0f),
    m_isOnGround(false) { }

u::map<int, int> &getKeyState(int key = 0, bool keyDown = false, bool keyUp = false);
void getMouseDelta(int *deltaX, int *deltaY);

void client::update(const kdMap &map) {
    u::vector<clientCommands> commands;
    inputGetCommands(commands);
    inputMouseMove();

    move(commands, map);
}

void client::move(const u::vector<clientCommands> &commands, const kdMap &map) {
    m::vec3 velocity;
    m::vec3 direction;
    m::vec3 side;
    m::vec3 newDirection(0.0f, 0.0f, 0.0f);
    m::vec3 jump(0.0f, 0.0f, 0.0f);
    m::quat rotation;

    rotation = m_rotation;
    rotation.getOrient(&direction, nullptr, &side);

    m::vec3 upCamera;
    for (auto &it : commands) {
        switch (it) {
            case kCommandForward:
                newDirection += direction;
                break;
            case kCommandBackward:
                newDirection -= direction;
                break;
            case kCommandLeft:
                newDirection -= side;
                break;
            case kCommandRight:
                newDirection += side;
                break;
            case kCommandUp:
                m_origin.y += 0.5f;
                break;
            case kCommandDown:
                m_origin.y -= 0.5f;
                break;
            case kCommandJump:
                jump = m::vec3(0.0f, 1.0f, 0.0f);
                break;
        }
    }

    const float clientSpeed = 2.0f;
    const float jumpSpeed = 30.0f;

    newDirection.y = 0.0f;
    if (newDirection.absSquared() > 0.1f)
        newDirection.setLength(clientSpeed);
    newDirection.y += velocity.y;
    if (m_isOnGround)
        newDirection += jump * jumpSpeed;
    m_velocity = newDirection;

    kdSphereTrace trace;
    trace.radius = 5.0f; // radius of sphere for client
    trace.start = m_origin;
    trace.dir = m_velocity;
    map.traceSphere(&trace);
    kdMap::clipVelocity(newDirection, trace.plane.n, m_velocity, kdMap::kOverClip);
    float fraction = m::clamp(trace.fraction, 0.0f, 1.0f);
    if (fraction > 0.0f)
        m_origin += trace.dir * fraction;
}

void client::inputMouseMove(void) {
    static const float kSensitivity = 0.50f / 6.0f;
    static const bool kInvert = true;
    const float invert = kInvert ? -1.0f : 1.0f;

    int deltaX;
    int deltaY;
    getMouseDelta(&deltaX, &deltaY);

    m_mouseLat -= (float)deltaY * kSensitivity * invert;
    m::clamp(m_mouseLat, -89.0f, 89.0f);

    m_mouseLon -= (float)deltaX * kSensitivity * invert;

    m::quat qlat(m::vec3::xAxis, m_mouseLat * m::kDegToRad);
    m::quat qlon(m::vec3::yAxis, m_mouseLon * m::kDegToRad);

    setRotation(qlon * qlat);
}

void client::inputGetCommands(u::vector<clientCommands> &commands) {
    u::map<int, int> &keyState = getKeyState();
    if (keyState[SDLK_w])     commands.push_back(kCommandForward);
    if (keyState[SDLK_s])     commands.push_back(kCommandBackward);
    if (keyState[SDLK_a])     commands.push_back(kCommandLeft);
    if (keyState[SDLK_d])     commands.push_back(kCommandRight);
    if (keyState[SDLK_SPACE]) commands.push_back(kCommandJump);
    if (keyState[SDLK_q])     commands.push_back(kCommandUp);
    if (keyState[SDLK_e])     commands.push_back(kCommandDown);
}

void client::setRotation(const m::quat &rotation) {
    m_rotation = rotation;
}

void client::getDirection(m::vec3 *direction, m::vec3 *up, m::vec3 *side) const {
    return m_rotation.getOrient(direction, up, side);
}

const m::vec3 &client::getPosition(void) const {
    return m_origin;
}
