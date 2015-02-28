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
    : m_colorTextureUnitLocation(0)
{
}

bool particleSystemMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/particles.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/particles.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");

    return true;
}

void particleSystemMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat*)wvp.m);
}

void particleSystemMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

///! particleSystem
particleSystem::particleSystem()
    : m_initFunction(nullptr)
    , m_gravity(1.0f)
    , m_vao(0)
    , m_vbo(0)
{
}

particleSystem::~particleSystem() {
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        gl::DeleteBuffers(1, &m_vbo);
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

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(1, &m_vbo);

    gl::BindVertexArray(m_vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(5)); // color

    if (!m_method.init())
        return false;

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
    m_vertices.reserve(m_particles.size() * 6);

    // Construct the vertices
    size_t count = 0;
    for (auto &it : m_particles) {
        // Ignore dead particles
        if (it.lifeTime < 0.0f)
            continue;
        // Billboard the particles
        const m::vec3 x = it.currentSize * 0.5f * side;
        const m::vec3 y = it.currentSize * 0.5f * up;
        // Deal with offsetting for individual particles in this system
        const m::vec3 q1 =  x + y + it.currentOrigin;
        const m::vec3 q2 = -x + y + it.currentOrigin;
        const m::vec3 q3 = -x - y + it.currentOrigin;
        const m::vec3 q4 =  x - y + it.currentOrigin;

        // 1, 2, 3, 3, 4, 1 (GL_TRIANGLES)
        m_vertices.push_back({q1.x, q1.y, q1.z, 0.0f, 0.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q2.x, q2.y, q2.z, 1.0f, 0.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q3.x, q3.y, q3.z, 1.0f, 1.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q3.x, q3.y, q3.z, 1.0f, 1.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q4.x, q4.y, q4.z, 0.0f, 1.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        m_vertices.push_back({q1.x, q1.y, q1.z, 0.0f, 0.0f, it.color.x, it.color.y, it.color.z, it.currentAlpha});
        count += 2;
    }
    if (count == 0)
        return;

    // Blast it out in one giant shot
    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(3)); // uv
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(5)); // color

    // Render!
    m_method.enable();

    m_method.setWVP(p.projection() * p.view() * p.world());
    m_texture.bind(GL_TEXTURE0);
    gl::Enable(GL_DEPTH_TEST);
    gl::DepthFunc(GL_LESS);
    gl::Disable(GL_CULL_FACE);
    gl::DepthMask(GL_FALSE);
    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl::DrawArrays(GL_TRIANGLES, 0, count);
    gl::CullFace(GL_BACK);
    gl::DepthMask(GL_TRUE);
}

void particleSystem::addParticle(particle &&p) {
    m_particles.push_back(p);
}

void particleSystem::initParticle(particle &p, const m::vec3 &ownerPosition) {
    m_initFunction(p, ownerPosition);
}

void particleSystem::update(const pipeline &p) {
    const float dt = p.delta() * 0.1f;
    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f) {
            if (it.respawn)
                initParticle(it, it.startOrigin);
            else
                continue;
        }
        it.currentOrigin += it.velocity * dt - m::vec3(0.0f, dt*dt*0.5f*m_gravity, 0.0f);
        it.velocity.y -= m_gravity*dt;
        it.lifeTime -= dt;
        const float f = it.lifeTime / it.totalLifeTime;
        const float scale = sinf(f / m::kPi);
        it.currentAlpha = it.startAlpha * scale;
        it.currentSize = scale * it.startSize + 0.1f;
    }
}

}
