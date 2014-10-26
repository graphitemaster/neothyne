#ifndef R_SPLASH_HDR
#define R_SPLASH_HDR
#include "r_common.h"
#include "r_pipeline.h"
#include "r_texture.h"
#include "r_method.h"
#include "r_quad.h"

#include "m_mat4.h"

#include "u_string.h"

namespace r {

struct splashMethod : method {
    bool init();

    void setTextureUnit(int unit);
    void setScreenSize(const m::perspectiveProjection &project);
    void setTime(float dt);

private:
    GLint m_splashTextureLocation;
    GLint m_screenSizeLocation;
    GLint m_timeLocation;
};

struct splashScreen {
    bool load(const u::string &splashScreen);
    bool upload();
    void render(const rendererPipeline &pipeline);

private:
    quad m_quad;
    texture2D m_texture;
    splashMethod m_method;
};

}

#endif