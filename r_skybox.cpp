#include "r_skybox.h"
#include "r_pipeline.h"

#include "u_string.h"

namespace r {
///! method
bool skyboxMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/skybox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/skybox.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_cubeMapLocation = getUniformLocation("gCubemap");

    return true;
}

void skyboxMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void skyboxMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_cubeMapLocation, unit);
}

void skyboxMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

///! renderer
bool skybox::load(const u::string &skyboxName) {
    return m_cubemap.load(skyboxName + "_ft", skyboxName + "_bk", skyboxName + "_up",
                          skyboxName + "_dn", skyboxName + "_rt", skyboxName + "_lf");
}

bool skybox::upload() {
    if (!m_cube.upload())
        return false;
    if (!m_cubemap.upload())
        return false;
    if (!m_method.init())
        return false;

    m_method.enable();
    m_method.setTextureUnit(0);

    return true;
}

void skybox::render(const pipeline &pl) {
    // Construct the matrix for the skybox
    pipeline wpl = pl;
    pipeline p;
    p.setWorld(pl.position());
    p.setPosition(pl.position());
    p.setRotation(pl.rotation());
    p.setPerspective(pl.perspective());

    m_method.enable();
    m_method.setWVP(p.projection() * p.view() * p.world());
    m_method.setWorld(wpl.world());

    // render skybox cube
    gl::DepthFunc(GL_LEQUAL);
    gl::CullFace(GL_FRONT);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture

    m_cube.render();

    gl::DepthFunc(GL_LESS);
    gl::CullFace(GL_BACK);
}

}
