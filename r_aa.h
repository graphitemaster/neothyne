#ifndef R_AA_HDR
#define R_AA_HDR
#include "r_common.h"
#include "r_method.h"

namespace m {
    struct mat4;
    struct perspective;
}

namespace r {

struct aaMethod : method {
    aaMethod();

    bool init(const u::initializer_list<const char *> &defines = { });

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_screenSizeLocation;
};

inline aaMethod::aaMethod()
    : m_WVPLocation(-1)
    , m_colorMapLocation(-1)
    , m_screenSizeLocation(-1)
{
}

struct aa {
    aa();
    ~aa();

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
