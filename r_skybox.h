#ifndef R_SKYBOX_HDR
#define R_SKYBOX_HDR
#include "r_texture.h"
#include "r_method.h"
#include "r_geom.h"

struct fog;

namespace u {
    struct string;
}

namespace m {
    struct mat4;
    struct vec3;
}

namespace r {

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
    uniform *m_cubeMap;
    uniform *m_world;
    uniform *m_skyColor;
    struct {
        uniform *color;
        uniform *density;
    } m_fog;
};

struct skybox {
    skybox();
    bool load(const u::string &skyboxName);
    bool upload();

    void render(const pipeline &pl, const fog &f);

private:
    texture3D m_cubemap;
    method *m_skyboxMethod;
    method *m_foggedSkyboxMethod;
    cube m_cube;
    m::vec3 m_skyColor;
};

}

#endif
