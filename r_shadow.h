#ifndef R_SHADOW_HDR
#define R_SHADOW_HDR
#include "r_common.h"
#include "r_method.h"
#include "m_mat.h"

namespace r {

struct shadowMap {
    shadowMap();
    ~shadowMap();

    bool init(size_t size);
    void update(size_t size);
    void bindWriting();

    GLuint texture() const;

    float widthScale() const;
    float heightScale() const;

private:
    void setSize(size_t size);

    size_t m_size;
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
