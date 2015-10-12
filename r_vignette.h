#ifndef R_VIGNETTE_HDR
#define R_VIGNETTE_HDR
#include "r_common.h"
#include "r_method.h"

namespace r {

struct vignetteMethod : method {
    vignetteMethod();

    bool init(const u::initializer_list<const char *> &defines = { });

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);
    void setProperties(float radius, float softness);

private:
    uniform *m_WVP;
    uniform *m_colorMap;
    uniform *m_screenSize;
    uniform *m_properties;
};

inline vignetteMethod::vignetteMethod()
    : m_WVP(nullptr)
    , m_colorMap(nullptr)
    , m_screenSize(nullptr)
    , m_properties(nullptr)
{
}

struct vignette {
    vignette();
    ~vignette();

    bool init(const m::perspective &p);

    void update(const m::perspective &p);
    void bindWriting();

    GLuint texture() const;

private:
    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

}

#endif
