#ifndef CLIENT_HDR
#define CLIENT_HDR
#include "util.h"
#include "math.h"
#include "kdmap.h"

enum clientCommands {
    kCommandForward,
    kCommandBackward,
    kCommandLeft,
    kCommandRight,
    kCommandJump
};

struct client {
    client();

    void update(const kdMap &map, float dt);
    void getDirection(m::vec3 *direction, m::vec3 *up, m::vec3 *side) const;
    const m::vec3 &getPosition(void) const;
    void setRotation(const m::quat &rotation);

private:
    void inputMouseMove(void);
    void inputGetCommands(u::vector<clientCommands> &commands);
    void move(const u::vector<clientCommands> &commands);

    bool tryUnstick(const kdMap &map, float radius);

    float m_mouseLat;
    float m_mouseLon;

    m::vec3 m_origin;
    m::vec3 m_velocity;
    m::quat m_rotation;

    bool m_isOnGround;
    bool m_isOnWall;
};

#endif
