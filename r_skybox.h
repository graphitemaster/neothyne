#ifndef R_SKYBOX_HDR
#define R_SKYBOX_HDR

#include "r_common.h"
#include "r_pipeline.h"
#include "r_texture.h"
#include "r_method.h"

#include "u_string.h"

namespace r {

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
    ~skybox();

    bool load(const u::string &skyboxName);
    bool upload();

    void render(const rendererPipeline &pipeline);

private:
    union {
        struct {
            GLuint m_vbo;
            GLuint m_ibo;
        };
        GLuint m_buffers[2];
    };
    GLuint m_vao;
    texture3D m_cubemap; // skybox cubemap
    skyboxMethod m_method;
};

}

#endif
