#ifndef R_BILLBOARD_HDR
#define R_BILLBOARD_HDR
#include "r_common.h"
#include "r_pipeline.h"
#include "r_texture.h"
#include "r_method.h"

#include "u_vector.h"
#include "u_string.h"

namespace r {

struct billboardMethod : method {
    virtual bool init();

    void setVP(const m::mat4 &vp);
    void setCamera(const m::vec3 &position);
    void setTextureUnit(int unit);
    void setSize(float width, float height);

private:
    GLuint m_VPLocation;
    GLuint m_cameraPositionLocation;
    GLuint m_colorMapLocation;
    GLuint m_sizeLocation;
};

struct billboard {
    billboard();
    ~billboard();

    bool load(const u::string &billboardTexture);
    bool upload();

    void render(const rendererPipeline &pipeline);

    // you must add all positions for this billboard before calling `upload'
    void add(const m::vec3 &position);

private:
    GLuint m_vbo;
    GLuint m_vao;
    u::vector<m::vec3> m_positions;
    texture2D m_texture;
    billboardMethod m_method;
};

}

#endif
