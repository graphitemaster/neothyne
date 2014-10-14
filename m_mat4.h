#ifndef M_MAT4_HDR
#define M_MAT4_HDR

namespace m {

struct vec3;
struct quat;

struct perspectiveProjection {
    float fov;
    float width;
    float height;
    float nearp;
    float farp;
};

struct mat4 {
    float m[4][4];

    void loadIdentity(void);
    mat4 inverse(void);
    mat4 operator*(const mat4 &t) const;
    void setScaleTrans(float scaleX, float scaleY, float scaleZ);
    void setRotateTrans(float rotateX, float rotateY, float rotateZ);
    void setTranslateTrans(float x, float y, float z);
    void setCameraTrans(const vec3 &target, const vec3 &up);
    void setCameraTrans(const vec3 &position, const quat &q);
    void setPersProjTrans(const perspectiveProjection &projection);
    void getOrient(vec3 *direction, vec3 *up, vec3 *side) const;
};

}

#endif
