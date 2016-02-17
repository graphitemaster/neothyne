#include "r_billboard.h"
#include "r_pipeline.h"

namespace r {

///! renderer
bool billboard::load(const u::string &billboardTexture) {
    if (!m_texture.load("<premul>" + billboardTexture))
        return false;
    return true;
}

bool billboard::upload() {
    if (!m_texture.upload())
        return false;

    m_method = &r::methods::instance()["billboarding"];

    geom::upload();

    gl::BindVertexArray(vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), u::offset_of(&vertex::position));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), u::offset_of(&vertex::coordinate));

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), 0, GL_DYNAMIC_DRAW);

    m_method->enable();
    m_method->getUniform("gColorMap")->set(0);

    return true;
}

void billboard::render(const pipeline &pl, float size) {
    pipeline p = pl;

    const m::quat rotation = p.rotation();
    m::vec3 up;
    m::vec3 side;
    rotation.getOrient(nullptr, &up, &side);

    m_vertices.destroy();
    m_vertices.reserve(m_entries.size() * 4);
    u::vector<GLuint> indices(m_entries.size() * 6);

    u::sort(m_entries.begin(), m_entries.end(),
        [&pl](const entry &lhs, const entry &rhs) {
            const float d1 = (lhs.position - pl.position()).abs();
            const float d2 = (rhs.position - pl.position()).abs();
            return d1 > d2;
        }
    );

    for (const auto &e : m_entries) {
        const auto &it = e.position;
        const m::vec3 x = size * 0.5f * ((e.flags & kSide) ? side : e.side);
        const m::vec3 y = size * 0.5f * ((e.flags & kUp) ? up : e.up);
        const m::vec3 q1 =  x + y + it;
        const m::vec3 q2 = -x + y + it;
        const m::vec3 q3 = -x - y + it;
        const m::vec3 q4 =  x - y + it;

        const size_t index = m_vertices.size();
        m_vertices.push_back({q1, {0.0f, 0.0f}});
        m_vertices.push_back({q2, {1.0f, 0.0f}});
        m_vertices.push_back({q3, {1.0f, 1.0f}});
        m_vertices.push_back({q4, {0.0f, 1.0f}});

        indices.push_back(index + 0);
        indices.push_back(index + 1);
        indices.push_back(index + 2);
        indices.push_back(index + 2);
        indices.push_back(index + 3);
        indices.push_back(index + 0);
    }
    if (indices.empty())
        return;

    m_memory = m_vertices.size() * sizeof(vertex);
    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_memory, &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), u::offset_of(&vertex::position));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), u::offset_of(&vertex::coordinate));

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof indices[0], &indices[0], GL_DYNAMIC_DRAW);
    m_memory += indices.size() * sizeof indices[0];

    m_method->enable();
    m_method->getUniform("gVP")->set(p.projection() * p.view());
    m_texture.bind(GL_TEXTURE0);
    gl::DrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    m_entries.clear();
}

void billboard::add(const m::vec3 &position,
                    int flags,
                    const m::vec3 &optionalSide,
                    const m::vec3 &optionalUp)
{
    m_entries.push_back({ position, flags, optionalSide, optionalUp });
}

}
