#include "r_particles.h"
#include "r_pipeline.h"

#include "u_string.h"
#include "u_misc.h"

namespace r {

///! particleSystemMethod
particleSystemMethod::particleSystemMethod()
    : m_VP(nullptr)
    , m_colorTextureUnit(nullptr)
    , m_depthTextureUnit(nullptr)
{
}

bool particleSystemMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/particles.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/particles.fs"))
        return false;
    if (!finalize({ "position", "color", "power" }))
        return false;

    m_VP = getUniform("gVP", uniform::kMat4);
    m_colorTextureUnit = getUniform("gColorMap", uniform::kSampler);
    m_depthTextureUnit = getUniform("gDepthMap", uniform::kSampler);

    post();
    return true;
}

void particleSystemMethod::setVP(const m::mat4 &vp) {
    m_VP->set(vp);
}

void particleSystemMethod::setColorTextureUnit(int unit) {
    m_colorTextureUnit->set(unit);
}

void particleSystemMethod::setDepthTextureUnit(int unit) {
    m_depthTextureUnit->set(unit);
}

///! particleSystem
particleSystem::particleSystem() {
}

particleSystem::~particleSystem() {
}

bool particleSystem::load(const u::string &file) {
    return m_texture.load(file);
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
    if (gl::has(gl::ARB_half_float_vertex)) {
        halfVertex *v = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, sizeof(halfVertex), 0, GL_DYNAMIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, sizeof(halfVertex), &v->x); // position
        gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(halfVertex), &v->rgb); // color
        gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(halfVertex), &v->power); // power
    } else {
        singleVertex *v = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, sizeof(singleVertex), 0, GL_DYNAMIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->position); // position
        gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->rgb); // color
        gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->power); // power
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint), 0, GL_DYNAMIC_DRAW);

    m_method.enable();
    m_method.setColorTextureUnit(0);
    m_method.setDepthTextureUnit(1);

    return true;
}

void particleSystem::render(const pipeline &pl) {
    pipeline p = pl;
    const m::quat rotation = p.rotation();
    m::vec3 side;
    m::vec3 up;
    rotation.getOrient(nullptr, &up, &side);

    if (gl::has(gl::ARB_half_float_vertex)) {
        m_halfVertices.destroy();
        m_halfVertices.reserve(m_particles.size() * 4);
    } else {
        m_singleVertices.destroy();
        m_singleVertices.reserve(m_particles.size() * 4);
    }

    m_indices.destroy();
    m_indices.reserve(m_particles.size() * 6);

    // sort particles by ones closest to camera
    u::sort(m_particles.begin(), m_particles.end(),
        [&pl](const particle &lhs, const particle &rhs) {
            const float d1 = (lhs.origin - pl.position()).abs();
            const float d2 = (rhs.origin - pl.position()).abs();
            return d1 > d2;
        }
    );

    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f)
            continue;

        const m::vec3 x = it.size * 0.5f * side;
        const m::vec3 y = it.size * 0.5f * up;
        const m::vec3 q[] = { x + y + it.origin,
                             -x + y + it.origin,
                             -x - y + it.origin,
                              x - y + it.origin };
        size_t index = 0;
        if (gl::has(gl::ARB_half_float_vertex)) {
            index = m_halfVertices.size();
            const auto &c = m::convertToHalf((const float *)q, 3*4);
            for (size_t i = 0; i < c.size(); i += 3)
                m_halfVertices.push_back({c[i], c[i+1], c[i+2], it.color, it.alpha, it.power});
        } else {
            index = m_singleVertices.size();
            for (const auto &jt : q)
                m_singleVertices.push_back({jt, it.color, it.alpha, it.power});
        }
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

    if (gl::has(gl::ARB_half_float_vertex)) {
        halfVertex *v = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, m_halfVertices.size() * sizeof(halfVertex), &m_halfVertices[0], GL_DYNAMIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, sizeof(halfVertex), &v->x); // position
        gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(halfVertex), &v->rgb); // color
        gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(halfVertex), &v->power); // power
    } else {
        singleVertex *v = nullptr;
        gl::BufferData(GL_ARRAY_BUFFER, m_singleVertices.size() * sizeof(singleVertex), &m_singleVertices[0], GL_DYNAMIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->position); // position
        gl::VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->rgb); // color
        gl::VertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(singleVertex), &v->power); // power
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * m_indices.size(),
        &m_indices[0], GL_DYNAMIC_DRAW);

    m_method.enable();
    m_method.setVP(p.projection() * p.view());
    m_texture.bind(GL_TEXTURE0);

    gl::Disable(GL_CULL_FACE);
    gl::DepthFunc(GL_LESS);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, nullptr);

    gl::Enable(GL_CULL_FACE);
}

void particleSystem::addParticle(particle &&p) {
    m_particles.push_back(p);
}

void particleSystem::update(const pipeline &p) {
    const float dt = p.delta() * 0.1f;
    const float gravity = getGravity();

    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f) {
            if (it.respawn)
                initParticle(it, p.position());
            else
                continue;
        }
        it.origin = it.origin + it.velocity*dt - m::vec3(0.0f, dt*dt*0.5f*gravity, 0.0f);
        it.velocity.y -= gravity*dt;
        it.lifeTime -= dt;
        const float f = it.lifeTime / it.totalLifeTime;
        const float scale = m::sin(f * m::kPi);
        it.alpha = it.startAlpha * scale;
        it.size = scale * it.startSize + 0.1f;
    }
}

}
