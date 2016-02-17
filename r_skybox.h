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
