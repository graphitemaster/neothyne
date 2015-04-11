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
    vec4 m[4];

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
};

}

#endif
