#ifndef R_SHADOW_HDR
#define R_SHADOW_HDR
#include "r_common.h"
#include "r_method.h"
#include "m_mat.h"

namespace r {

struct shadowMap {
    shadowMap();
    ~shadowMap();

    bool init(const m::perspective &p);
    void update(const m::perspective &p);
    void bindWriting();

    GLuint texture() const;

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
    GLint m_WVPLocation;
};

struct shadowMapDebugMethod : method {
    shadowMapDebugMethod();
    bool init();
    void setShadowMapTextureUnit(int unit);
    void setWVP(const m::mat4 &wvp);
    void setPerspective(const m::perspective &p);
private:
    GLint m_WVPLocation;
    GLint m_screenSizeLocation;
    GLint m_screenFrustumLocation;
    GLint m_shadowMapTextureUnitLocation;
};

}

#endif