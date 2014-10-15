#include "r_splash.h"

namespace r {
///! method
bool splashMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/splash.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/splash.fs"))
        return false;
    if (!finalize())
        return false;

    m_splashTextureLocation = getUniformLocation("gSplashTexture");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_timeLocation = getUniformLocation("gTime");

    return true;
}

void splashMethod::setScreenSize(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
}

void splashMethod::setTime(float dt) {
    gl::Uniform1f(m_timeLocation, dt);
}

void splashMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_splashTextureLocation, unit);
}

///! renderer
bool splashScreen::load(const u::string &splashScreen) {
    return m_texture.load(splashScreen);
}

bool splashScreen::upload() {
    if (!m_texture.upload() || !m_quad.upload())
        return false;
    if (!m_method.init())
        return false;

    m_method.enable();
    m_method.setTextureUnit(0);

    return true;
}

void splashScreen::render(const rendererPipeline &pipeline) {
    gl::Disable(GL_CULL_FACE);
    gl::Disable(GL_DEPTH_TEST);

    m_method.enable();
    m_method.setScreenSize(pipeline.getPerspectiveProjection());
    m_method.setTime(pipeline.getTime());

    m_texture.bind(GL_TEXTURE0);
    m_quad.render();

    gl::Enable(GL_DEPTH_TEST);
    gl::Enable(GL_CULL_FACE);
}

}
