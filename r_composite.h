#ifndef R_COMPOSITE_HDR
#define R_COMPOSITE_HDR
#include "r_method.h"

namespace m {

struct mat4;
struct perspective;

}

namespace r {

struct compositeMethod : method {
    compositeMethod();
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());
    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setColorGradingTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);

private:
    uniform *m_WVP;
    uniform *m_colorMap;
    uniform *m_colorGradingMap;
    uniform *m_screenSize;
};

inline compositeMethod::compositeMethod()
    : m_WVP(nullptr)
    , m_colorMap(nullptr)
    , m_colorGradingMap(nullptr)
    , m_screenSize(nullptr)
{
}

struct composite {
    composite();
    ~composite();

    bool init(const m::perspective &p, GLuint depth);
    void update(const m::perspective &p);
    void bindWriting();

    GLuint texture() const;

private:
    enum {
        kBuffer,
        kDepth
    };

    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

}

#endif
