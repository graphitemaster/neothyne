#include "r_skybox.h"

namespace r {
///! method
bool skyboxMethod::init(void) {
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
skybox::~skybox(void) {
    gl::DeleteBuffers(2, m_buffers);
    gl::DeleteVertexArrays(1, &m_vao);
}

bool skybox::load(const u::string &skyboxName) {
    return m_cubemap.load(skyboxName + "_ft", skyboxName + "_bk", skyboxName + "_up",
                          skyboxName + "_dn", skyboxName + "_rt", skyboxName + "_lf");
}

bool skybox::upload(void) {
    if (!m_cubemap.upload())
        return false;
    if (!m_method.init())
        return false;

    GLfloat vertices[] = {
        -1.0, -1.0,  1.0,
        1.0, -1.0,  1.0,
        -1.0,  1.0,  1.0,
        1.0,  1.0,  1.0,
        -1.0, -1.0, -1.0,
        1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        1.0,  1.0, -1.0,
    };

    GLubyte indices[] = { 0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1 };

    // create vao to encapsulate state
    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    gl::EnableVertexAttribArray(0);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    m_method.enable();
    m_method.setTextureUnit(0);

    return true;
}

void skybox::render(const rendererPipeline &pipeline) {
    // Construct the matrix for the skybox
    rendererPipeline worldPipeline = pipeline;
    rendererPipeline p;
    p.setWorldPosition(pipeline.getPosition());
    p.setPosition(pipeline.getPosition());
    p.setRotation(pipeline.getRotation());
    p.setPerspectiveProjection(pipeline.getPerspectiveProjection());

    m_method.enable();
    m_method.setWVP(p.getWVPTransform());
    m_method.setWorld(worldPipeline.getWorldTransform());

    // render skybox cube
    gl::CullFace(GL_FRONT);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture

    gl::BindVertexArray(m_vao);
    gl::DrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_BYTE, (void *)0);
    gl::BindVertexArray(0);
    gl::CullFace(GL_BACK);
}

}
