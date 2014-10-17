#include "r_billboard.h"

namespace r {

///! method
bool billboardMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/billboard.vs"))
        return false;
    if (!addShader(GL_GEOMETRY_SHADER, "shaders/billboard.gs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/billboard.fs"))
        return false;
    if (!finalize())
        return false;

    m_VPLocation = getUniformLocation("gVP");
    m_cameraPositionLocation = getUniformLocation("gCameraPosition");
    m_colorMapLocation = getUniformLocation("gColorMap");
    m_sizeLocation = getUniformLocation("gSize");

    return true;
}

void billboardMethod::setVP(const m::mat4 &vp) {
    gl::UniformMatrix4fv(m_VPLocation, 1, GL_TRUE, (const GLfloat*)vp.m);
}

void billboardMethod::setCamera(const m::vec3 &cameraPosition) {
    gl::Uniform3fv(m_cameraPositionLocation, 1, &cameraPosition.x);
}

void billboardMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

void billboardMethod::setSize(float width, float height) {
    gl::Uniform2f(m_sizeLocation, width, height);
}

///! renderer
billboard::billboard()
    : m_vbo(0)
    , m_vao(0)
{
}

billboard::~billboard() {
    if (m_vbo)
        gl::DeleteBuffers(1, &m_vbo);
    if (m_vbo)
        gl::DeleteVertexArrays(1, &m_vao);
}

bool billboard::load(const u::string &billboardTexture) {
    if (!m_texture.load(billboardTexture))
        return false;
    return true;
}

bool billboard::upload() {
    if (!m_texture.upload())
        return false;
    if (!m_method.init())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(1, &m_vbo);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(m::vec3) * m_positions.size(), &m_positions[0], GL_STATIC_DRAW);

    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, GL_OFFSET(0));
    gl::EnableVertexAttribArray(0);

    m_method.enable();
    m_method.setSize(16, 16);
    m_method.setTextureUnit(0);

    return true;
}

void billboard::render(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    m_method.enable();
    m_method.setCamera(p.getPosition());
    m_method.setVP(p.getVPTransform());

    m_texture.bind(GL_TEXTURE0);

    gl::BindVertexArray(m_vao);
    gl::DrawArrays(GL_POINTS, 0, m_positions.size());
    gl::BindVertexArray(0);
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

}
