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
billboard::billboard()
    : m_stats(r::stat::add("billboard", "Billboards"))
{
}

billboard::~billboard() {
    m_stats->decTextureCount();
    m_stats->decTextureMemory(m_texture.memory());
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

    m_method.enable();
    m_method.setColorTextureUnit(0);

    m_stats->incTextureCount();
    m_stats->incTextureMemory(m_texture.memory());

    return true;
}

void billboard::render(const pipeline &pl, float size) {
    const m::quat rotation = pl.rotation();
    m::vec3 up;
    m::vec3 side;
    rotation.getOrient(nullptr, &up, &side);

    m_stats->decVBOMemory(sizeof m_vertices[0] * m_vertices.size());
    m_stats->decIBOMemory(sizeof m_indices[0] * m_indices.size());

    m_vertices.destroy();
    m_vertices.reserve(m_entries.size() * 4);
    m_indices.destroy();
    m_indices.reserve(m_entries.size() * 6);

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

        m_indices.push_back(index + 0);
        m_indices.push_back(index + 1);
        m_indices.push_back(index + 2);
        m_indices.push_back(index + 2);
        m_indices.push_back(index + 3);
        m_indices.push_back(index + 0);
    }
    if (m_indices.empty())
        return;

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof m_vertices[0], &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof m_vertices[0], u::offset_of(&vertex::position));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof m_vertices[0], u::offset_of(&vertex::coordinate));

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof m_indices[0], &m_indices[0], GL_DYNAMIC_DRAW);

    m_stats->incVBOMemory(sizeof(vertex) * m_vertices.size());
    m_stats->incIBOMemory(sizeof(GLuint) * m_indices.size());

    m_method.enable();
    m_method.setVP(pl.projection() * pl.view());
    m_texture.bind(GL_TEXTURE0);
    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, nullptr);
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
