#include "r_billboard.h"
#include "r_pipeline.h"

namespace r {

///! method
bool billboardMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/billboard.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/billboard.fs"))
        return false;
    if (!finalize())
        return false;

    m_VPLocation = getUniformLocation("gVP");
    m_colorMapLocation = getUniformLocation("gColorMap");

    return true;
}

void billboardMethod::setVP(const m::mat4 &vp) {
    gl::UniformMatrix4fv(m_VPLocation, 1, GL_TRUE, (const GLfloat*)vp.m);
}

void billboardMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
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
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
}

bool billboard::load(const u::string &billboardTexture) {
    if (!m_texture.load("<premul>" + billboardTexture))
        return false;
    return true;
}

bool billboard::upload() {
    if (!m_texture.upload())
        return false;
    if (!m_method.init())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(1, &m_vbo);

    gl::BindVertexArray(m_vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv

    m_method.enable();
    m_method.setColorTextureUnit(0);

    return true;
}

void billboard::render(const pipeline &pl, float size) {
    pipeline p = pl;

    const m::quat rotation = p.rotation();
    m::vec3 up;
    m::vec3 side;
    rotation.getOrient(nullptr, &up, &side);

    m_vertices.destroy();
    m_vertices.reserve(m_positions.size() * 6);

    size_t count = 0;
    for (auto &it : m_positions) {
        const m::vec3 x = size * 0.5f * side;
        const m::vec3 y = size * 0.5f * up;
        const m::vec3 q1 =  x + y + it;
        const m::vec3 q2 = -x + y + it;
        const m::vec3 q3 = -x - y + it;
        const m::vec3 q4 =  x - y + it;

        m_vertices.push_back({q1, 0.0f, 0.0f});
        m_vertices.push_back({q2, 1.0f, 0.0f});
        m_vertices.push_back({q3, 1.0f, 1.0f});
        m_vertices.push_back({q3, 1.0f, 1.0f});
        m_vertices.push_back({q4, 0.0f, 1.0f});
        m_vertices.push_back({q1, 0.0f, 0.0f});
        count += 6;
    }
    if (count == 0)
        return;

    // Blast it out in one giant shot
    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv

    // Render
    m_method.enable();

    m_method.setVP(p.projection() * p.view());
    m_texture.bind(GL_TEXTURE0);

    gl::DrawArrays(GL_TRIANGLES, 0, count);

    m_positions.clear();
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

}
