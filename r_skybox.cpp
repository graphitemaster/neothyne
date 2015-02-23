#include "cvar.h"
#include "world.h"

#include "r_skybox.h"
#include "r_pipeline.h"

#include "u_string.h"
#include "u_vector.h"

namespace r {
///! method
bool skyboxMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/skybox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/skybox.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_cubeMapLocation = getUniformLocation("gCubemap");
    m_fogLocation.color = getUniformLocation("gFog.color");
    m_fogLocation.density = getUniformLocation("gFog.density");

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

void skyboxMethod::setFog(const fog &f) {
    gl::Uniform3fv(m_fogLocation.color, 1, &f.color.x);
    gl::Uniform1f(m_fogLocation.density, f.density);
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
    if (!m_methods[0].init())
        return false;
    if (!m_methods[1].init({"USE_FOG"}))
        return false;
    for (auto &it : m_methods) {
        it.enable();
        it.setTextureUnit(0);
    }
    return true;
}

void skybox::render(const pipeline &pl, const fog &f) {
    // Construct the matrix for the skybox
    pipeline wpl = pl;
    pipeline p;
    p.setWorld(pl.position());
    p.setPosition(pl.position());
    p.setRotation(pl.rotation());
    p.setPerspective(pl.perspective());

    skyboxMethod *renderMethod = nullptr;
    if (varGet<int>("r_fog")) {
        renderMethod = &m_methods[1];
        renderMethod->enable();
        renderMethod->setFog(f);
    } else {
        renderMethod = &m_methods[0];
        renderMethod->enable();
    }

    renderMethod->setWVP(p.projection() * p.view() * p.world());
    renderMethod->setWorld(wpl.world());

    // render skybox cube
    gl::DepthFunc(GL_LEQUAL);
    gl::CullFace(GL_FRONT);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture

    m_cube.render();

    gl::DepthFunc(GL_LESS);
    gl::CullFace(GL_BACK);
}

}
