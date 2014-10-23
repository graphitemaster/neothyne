#ifndef M_QUAT_HDR
#define M_QUAT_HDR

namespace m {

struct vec3;

struct quat {
    float x;
    float y;
    float z;
    float w;

    constexpr quat()
        : x(0.0f)
        , y(0.0f)
        , z(0.0f)
        , w(1.0f)
    {
    }

    constexpr quat(float x, float y, float z, float w)
        : x(x)
        , y(y)
        , z(z)
        , w(w)
    {
    }

    quat(const vec3 &vec, float angle) {
        rotationAxis(vec, angle);
    }

    quat conjugate() const {
        return quat(-x, -y, -z, w);
    }

    void endianSwap();

    // get all 3 axis of the quaternion
    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;

    friend quat operator*(const quat &l, const vec3 &v);
    friend quat operator*(const quat &l, const quat &r);

private:
    void rotationAxis(const vec3 &vec, float angle);
};

}

#endif
