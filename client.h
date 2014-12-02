#ifndef CLIENT_HDR
#define CLIENT_HDR
#include "u_vector.h"

#include "m_vec3.h"
#include "m_quat.h"

struct kdMap;

enum clientCommands {
    kCommandForward,
    kCommandBackward,
    kCommandLeft,
    kCommandRight,
    kCommandJump,
    kCommandCrouch
};

struct client {
    client();

    void update(const kdMap &map, float dt);
    void getDirection(m::vec3 *direction, m::vec3 *up, m::vec3 *side) const;
    m::vec3 getPosition() const;
    void setRotation(const m::quat &rotation);
    const m::quat &getRotation() const;

private:
    void inputMouseMove();
    void inputGetCommands(u::vector<clientCommands> &commands);
    void move(float dt, const u::vector<clientCommands> &commands);

    float m_mouseLat;
    float m_mouseLon;
    float m_viewHeight;

    m::vec3 m_origin;
    m::vec3 m_velocity;
    m::quat m_rotation;

    m::vec3 m_lastDirection;

    bool m_isOnGround;
    bool m_isOnWall;
    bool m_isCrouching;
};

#endif
