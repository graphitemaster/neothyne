#ifndef R_AA_HDR
#define R_AA_HDR
#include "r_common.h"
#include "r_method.h"

namespace m {
    struct mat4;
    struct perspective;
}

namespace r {

struct fxaaMethod : method {
    fxaaMethod();

    bool init(const u::initializer_list<const char *> &defines = { });

    void setWVP(const m::mat4 &wvp);
    void setColorTextureUnit(int unit);
    void setPerspective(const m::perspective &perspective);

private:
    GLint m_WVPLocation;
    GLint m_colorMapLocation;
    GLint m_screenSizeLocation;
};

inline fxaaMethod::fxaaMethod()
    : m_WVPLocation(-1)
    , m_colorMapLocation(-1)
    , m_screenSizeLocation(-1)
{
}

struct fxaa {
    fxaa();
    ~fxaa();

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

struct smaaMethod : method {
    smaaMethod();
    bool init(const char *vertex, const char *fragment, const u::initializer_list<const char *> &defines);
    void setWVP(const m::mat4 &wvp);
    void setPerspective(const m::perspective &perspective);
private:
    friend struct smaaEdgeMethod;
    friend struct smaaBlendMethod;
    friend struct smaaNeighborMethod;
    GLint m_WVPLocation;
    GLint m_screenSizeLocation;
};

struct smaaEdgeMethod : smaaMethod {
    smaaEdgeMethod();
    bool init(const u::initializer_list<const char *> &defines);
    void setAlbedoTextureUnit(int unit);
private:
    GLint m_albedoMapLocation;
};

struct smaaBlendMethod : smaaMethod {
    smaaBlendMethod();
    bool init(const u::initializer_list<const char *> &defines);
    void setEdgeTextureUnit(int unit);
    void setAreaTextureUnit(int unit);
    void setSearchTextureUnit(int unit);
private:
    GLint m_edgeMapLocation;
    GLint m_areaMapLocation;
    GLint m_searchMapLocation;
};

struct smaaNeighborMethod : smaaMethod {
    smaaNeighborMethod();
    bool init(const u::initializer_list<const char *> &defines);
    void setAlbedoTextureUnit(int unit);
    void setBlendTextureUnit(int unit);
private:
    GLint m_albedoMapLocation;
    GLint m_blendMapLocation;
};

inline smaaMethod::smaaMethod()
    : m_WVPLocation(-1)
    , m_screenSizeLocation(-1)
{
}

inline smaaEdgeMethod::smaaEdgeMethod()
    : m_albedoMapLocation(-1)
{
}

inline smaaBlendMethod::smaaBlendMethod()
    : m_edgeMapLocation(-1)
    , m_areaMapLocation(-1)
    , m_searchMapLocation(-1)
{
}

inline smaaNeighborMethod::smaaNeighborMethod()
    : m_albedoMapLocation(-1)
    , m_blendMapLocation(-1)
{
}

struct smaaMethods {
    static smaaMethods &instance();
    bool init();

    enum quality {
        kLow,
        kMedium,
        kHigh,
        kUltra
    };

    smaaEdgeMethod &edge(quality q);
    smaaBlendMethod &blend(quality q);
    smaaNeighborMethod &neighbor(quality q);

private:
    smaaMethods();
    smaaMethods(const smaaMethods &) = delete;
    void operator =(const smaaMethods &) = delete;

    u::vector<smaaEdgeMethod> m_edgeMethods;
    u::vector<smaaBlendMethod> m_blendMethods;
    u::vector<smaaNeighborMethod> m_neighborMethods;
    bool m_initialized;
    static smaaMethods m_instance;
};

struct smaa {
    smaa();
    ~smaa();

    bool init(const m::perspective &p);
    void update(const m::perspective &p);

    enum {
        kEdgeFBO,
        kBlendFBO,
        kNeighborFBO,
        kCountFBO
    };

    enum {
        kEdgeTex,
        kBlendTex,
        kNeighborTex,
        kAreaTex,
        kSearchTex,
        kCountTex
    };

    void bindWriting(int fbo);
    GLuint texture(int tex);

private:

    GLuint m_fbos[kCountFBO];
    GLuint m_textures[kCountTex];

    size_t m_width;
    size_t m_height;
};

}

#endif
