#ifndef R_SKYBOX_HDR
#define R_SKYBOX_HDR
#include "r_texture.h"
#include "r_method.h"
#include "r_geom.h"

namespace u {
    struct string;
}

namespace m {
    struct mat4;
}

namespace r {

struct pipeline;

struct skyboxMethod : method {
    bool init();

    void setWVP(const m::mat4 &wvp);
    void setTextureUnit(int unit);
    void setWorld(const m::mat4 &worldInverse);

private:
    GLint m_WVPLocation;
    GLint m_cubeMapLocation;
    GLint m_worldLocation;
};

struct skybox {
    bool load(const u::string &skyboxName);
    bool upload();

    void render(const pipeline &pl);

private:
    texture3D m_cubemap; // skybox cubemap
    skyboxMethod m_method;
    cube m_cube;
};

}

#endif
