#include "math.h"
namespace m {
    ///! vec3
    const vec3 vec3::xAxis(1.0f, 0.0f, 0.0f);
    const vec3 vec3::yAxis(0.0f, 1.0f, 0.0f);
    const vec3 vec3::zAxis(0.0f, 0.0f, 1.0f);

    void vec3::rotate(float angle, const vec3 &axe) {
        const float s = sinf(m::toRadian(angle / 2.0f));
        const float c = cosf(m::toRadian(angle / 2.0f));
        const float rx = axe.x * s;
        const float ry = axe.y * s;
        const float rz = axe.z * s;
        const float rw = c;
        quat rotateQ(rx, ry, rz, rw);
        quat conjugateQ = rotateQ.conjugate();
        quat W = rotateQ * (*this) * conjugateQ;
        x = W.x, y = W.y, z = W.z;
    }
}
