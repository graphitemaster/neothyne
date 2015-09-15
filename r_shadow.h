#ifndef R_SHADOW_HDR
#define R_SHADOW_HDR
#include "r_common.h"
#include "r_method.h"
#include "m_mat.h"

namespace r {

struct shadowMap {
    shadowMap();
    ~shadowMap();

    bool init(size_t width, size_t height);
    void update(size_t width, size_t height);
    void bindWriting();

    GLuint texture() const;

    float widthScale(size_t size) const;
    float heightScale(size_t size) const;

private:
    size_t m_width;
    size_t m_height;
    GLuint m_fbo;
    GLuint m_shadowMap;
};

struct shadowMapMethod : method {
    shadowMapMethod();
    bool init();
    void setWVP(const m::mat4 &wvp);
private:
    uniform *m_WVP;
};

}

#endif
