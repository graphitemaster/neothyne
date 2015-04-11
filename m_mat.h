#ifndef M_MAT_HDR
#define M_MAT_HDR
#include "m_vec.h"

namespace m {

struct quat;

struct perspective {
    perspective();
    float fov;
    float width;
    float height;
    float nearp;
    float farp;
};

inline perspective::perspective()
    : fov(0.0f)
    , width(0.0f)
    , height(0.0f)
    , nearp(0.0f)
    , farp(0.0f)
{
}

struct mat4 {
    vec4 a, b, c, d;

    void loadIdentity();
    mat4 inverse();
    mat4 operator*(const mat4 &t) const;
    void setScaleTrans(float scaleX, float scaleY, float scaleZ);
    void setRotateTrans(float rotateX, float rotateY, float rotateZ);
    void setTranslateTrans(float x, float y, float z);
    void setCameraTrans(const vec3 &target, const vec3 &up);
    void setCameraTrans(const vec3 &position, const quat &q);
    void setPerspectiveTrans(const perspective &p);
    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;

    float *ptr();
    const float *ptr() const;

private:
    static float det2x2(float a, float b, float c, float d);
    static float det3x3(float a1, float a2, float a3,
                        float b1, float b2, float b3,
                        float c1, float c2, float c3);
};

inline float *mat4::ptr() {
    return &a.x;
}

inline const float *mat4::ptr() const {
    return &a.x;
}

}

#endif
