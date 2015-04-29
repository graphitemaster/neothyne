#include "r_particles.h"
#include "r_pipeline.h"

#include "u_string.h"
#include "u_misc.h"

namespace r {

///! particle
particle::particle()
    : currentSize(1.0f)
    , startSize(1.0f)
    , currentAlpha(1.0f)
    , startAlpha(1.0f)
    , lifeTime(1.0f)
    , totalLifeTime(2.0f)
    , respawn(false)
{
}

///! particleSystemMethod
particleSystemMethod::particleSystemMethod()
    : m_VPLocation(0)
    , m_colorTextureUnitLocation(0)
{
}

bool particleSystemMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/particles.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/particles.fs"))
        return false;

    if (!finalize({ { 0, "position" },
                    { 1, "texCoord" },
                    { 2, "color"    } }))
    {
        return false;
    }

    m_VPLocation = getUniformLocation("gVP");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");

    return true;
}

void particleSystemMethod::setVP(const m::mat4 &vp) {
    gl::UniformMatrix4fv(m_VPLocation, 1, GL_TRUE, vp.ptr());
}

void particleSystemMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

///! particleSystem
particleSystem::particleSystem()
    : m_initFunction(nullptr)
    , m_gravity(1.0f)
{
}

bool particleSystem::load(const u::string &file, initParticleFunction initFunction) {
    if (initFunction) {
        m_initFunction = initFunction;
        if (m_texture.load(file))
            return true;
    }
    return false;
}

bool particleSystem::upload() {
    if (!m_texture.upload())
        return false;

    if (!m_method.init())
        return false;

    geom::upload();

    gl::BindVertexArray(vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);

    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(5)); // color

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), 0, GL_DYNAMIC_DRAW);

    m_method.enable();
    m_method.setColorTextureUnit(0);

    return true;
}

void particleSystem::render(const pipeline &pl) {
    pipeline p = pl;
    const m::quat rotation = p.rotation();
    m::vec3 side;
    m::vec3 up;
    rotation.getOrient(nullptr, &up, &side);

    m_vertices.destroy();
    m_vertices.reserve(m_particles.size() * 4);

    u::vector<GLuint> indices;
    indices.reserve(m_particles.size() * 6);

    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f)
            continue;

        const m::vec3 x = it.currentSize * 0.5f * side;
        const m::vec3 y = it.currentSize * 0.5f * up;
        const m::vec3 q1 =  x + y + it.currentOrigin;
        const m::vec3 q2 = -x + y + it.currentOrigin;
        const m::vec3 q3 = -x - y + it.currentOrigin;
        const m::vec3 q4 =  x - y + it.currentOrigin;

        const size_t index = m_vertices.size();
        m_vertices.push_back({q1, 0.0f, 0.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q2, 1.0f, 0.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q3, 1.0f, 1.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q4, 0.0f, 1.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});

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
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(5)); // color

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * indices.size(), &indices[0], GL_DYNAMIC_DRAW);

    m_method.enable();
    m_method.setVP(p.projection() * p.view());
    m_texture.bind(GL_TEXTURE0);
    gl::Disable(GL_CULL_FACE);
    gl::DepthMask(GL_FALSE);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl::DrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, nullptr);
    gl::Enable(GL_CULL_FACE);
    gl::DepthMask(GL_TRUE);
}

void particleSystem::addParticle(particle &&p) {
    m_particles.push_back(p);
}

void particleSystem::update(const pipeline &p) {
    const float dt = p.delta() * 0.1f;
    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f) {
            if (it.respawn)
                m_initFunction(it);
            else
                continue;
        }
        it.currentOrigin += it.velocity * dt - m::vec3(0.0f, dt*dt*0.5f*m_gravity, 0.0f);
        it.velocity.y -= m_gravity*dt;
        it.lifeTime -= dt;
        const float f = it.lifeTime / it.totalLifeTime;
        const float scale = m::sin(f / m::kPi);
        it.currentAlpha = it.startAlpha * scale;
        it.currentSize = scale * it.startSize + 0.1f;
    }
}

}
