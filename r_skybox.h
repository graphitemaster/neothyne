#ifndef R_SKYBOX_HDR
#define R_SKYBOX_HDR
#include "r_texture.h"
#include "r_method.h"

namespace u {
    struct string;
}

namespace m {
    struct mat4;
    struct vec3;
}

namespace r {

// fog
struct fog {
    fog();
    m::vec3 color;
    float density; // Used for Exp, Exp2, and sky fog gradient
    float start; // Starting range (linear only)
    float end; // Ending range (linear only)
    enum {
        kLinear,
        kExp,
        kExp2
    };
    int equation;
};

inline fog::fog()
    : density(0.0f)
    , start(0.0f)
    , end(0.0f)
    , equation(fog::kLinear)
{
}

struct pipeline;

struct skyboxMethod : method {
    bool init(const u::vector<const char *> &defines = u::vector<const char *>());

    void setWVP(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setWorld(const m::mat4 &worldInverse);
    void setFog(const fog &f);
    void setSkyColor(const m::vec3 &color);

private:
    uniform *m_WVP;
    uniform *m_world;
    uniform *m_skyColor;
    uniform *m_colorMap;
    struct {
        uniform *color;
        uniform *density;
    } m_fog;
};

struct skybox {
    skybox();
    ~skybox();
    bool load(const u::string &skyboxName);
    bool upload();

    void render(const pipeline &pl, const fog &f);

private:
    texture2D m_textures[6];
    skyboxMethod m_methods[2];
    m::vec3 m_skyColor;
    GLuint m_vao;
    GLuint m_vbo;
};

}

#endif
