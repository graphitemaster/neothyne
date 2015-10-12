#include "r_billboard.h"
#include "r_pipeline.h"

namespace r {

///! method
billboardMethod::billboardMethod()
    : m_VP(nullptr)
    , m_colorMap(nullptr)
{
}

bool billboardMethod::init() {
    if (!method::init("billboarding"))
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/billboard.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/billboard.fs"))
        return false;
    if (!finalize({ "position", "texCoord" }))
        return false;

    m_VP = getUniform("gVP", uniform::kMat4);
    m_colorMap = getUniform("gColorMap", uniform::kSampler);

    post();
    return true;
}

void billboardMethod::setVP(const m::mat4 &vp) {
    m_VP->set(vp);
}

void billboardMethod::setColorTextureUnit(int unit) {
    m_colorMap->set(unit);
}

///! renderer
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

    geom::upload();

    gl::BindVertexArray(vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), 0, GL_DYNAMIC_DRAW);

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
    m_vertices.reserve(m_positions.size() * 4);

    u::vector<GLuint> indices;
    indices.reserve(m_positions.size() * 6);

    u::sort(m_positions.begin(), m_positions.end(),
        [&pl](const m::vec3 &lhs, const m::vec3 &rhs) {
            const float d1 = (lhs - pl.position()).abs();
            const float d2 = (rhs - pl.position()).abs();
            return d1 > d2;
        }
    );

    for (const auto &it : m_positions) {
        const m::vec3 x = size * 0.5f * side;
        const m::vec3 y = size * 0.5f * up;
        const m::vec3 q1 =  x + y + it;
        const m::vec3 q2 = -x + y + it;
        const m::vec3 q3 = -x - y + it;
        const m::vec3 q4 =  x - y + it;

        const size_t index = m_vertices.size();
        m_vertices.push_back({q1, 0.0f, 0.0f});
        m_vertices.push_back({q2, 1.0f, 0.0f});
        m_vertices.push_back({q3, 1.0f, 1.0f});
        m_vertices.push_back({q4, 0.0f, 1.0f});

        indices.push_back(index + 0);
        indices.push_back(index + 1);
        indices.push_back(index + 2);
        indices.push_back(index + 2);
        indices.push_back(index + 3);
        indices.push_back(index + 0);
    }
    if (indices.empty())
        return;

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), &indices[0], GL_DYNAMIC_DRAW);

    m_method.enable();
    m_method.setVP(p.projection() * p.view());
    m_texture.bind(GL_TEXTURE0);
    gl::DrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    m_positions.clear();
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

}
