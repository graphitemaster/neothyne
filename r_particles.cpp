#include <limits.h>

#include "r_particles.h"
#include "r_pipeline.h"

#include "u_string.h"
#include "u_misc.h"

#include "m_plane.h"

#include "c_variable.h"

namespace r {

VAR(int, r_particle_max_resolution, "maximum particle resolution", 16, 512, 128);

///! particleSystemMethod
particleSystemMethod::particleSystemMethod()
    : m_VP(nullptr)
    , m_colorTextureUnit(nullptr)
    , m_depthTextureUnit(nullptr)
    , m_power(nullptr)
    , m_screenSize(nullptr)
{
}

bool particleSystemMethod::init() {
    if (!method::init("particle system"))
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
    m_power = getUniform("gPower", uniform::kFloat);
    m_screenSize = getUniform("gScreenSize", uniform::kVec2);

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

void particleSystemMethod::setPerspective(const m::perspective &p) {
    m_screenSize->set(m::vec2(p.width, p.height));
}

void particleSystemMethod::setPower(float power) {
    m_power->set(power);
}

///! particleSystem
particleSystem::particleSystem()
    : m_vao(0)
    , m_bufferIndex(0)
    , m_stats(r::stat::add("particle", "Particle Systems"))
{
    memset(m_buffers, 0, sizeof m_buffers);

}
particleSystem::~particleSystem() {
    m_stats->decTextureCount();
    m_stats->decTextureMemory(m_texture.memory());

    if (m_buffers[0])
        gl::DeleteBuffers(sizeof m_buffers / sizeof *m_buffers, m_buffers);
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
}

bool particleSystem::load(const u::string &file) {
    const bool read = m_texture.load(file);
    if (read) {
        // Limit the size of loaded particle textures to reduce over draw in blending
        const size_t w = m_texture.width();
        const size_t h = m_texture.height();
        if (w != h) {
            // Particle textures must be power of two
            return false;
        }
        if (int(w) > r_particle_max_resolution)
            m_texture.resize(r_particle_max_resolution, r_particle_max_resolution);
        return true;
    }
    return false;
}

bool particleSystem::upload() {
    if (!m_texture.upload())
        return false;
    if (!m_method.init())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(sizeof m_buffers / sizeof *m_buffers, m_buffers);

    gl::BindVertexArray(m_vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    for (size_t i = 0; i < sizeof m_vbos / sizeof *m_vbos; i++) {
        gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[i]);
        if (gl::has(gl::ARB_half_float_vertex)) {
            gl::BufferData(GL_ARRAY_BUFFER, sizeof(halfVertex), 0, GL_STREAM_DRAW);
            gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT,    GL_FALSE, sizeof(halfVertex), u::offset_of(&halfVertex::position)); // position
            gl::VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(halfVertex), u::offset_of(&halfVertex::color)); // color
        } else {
            gl::BufferData(GL_ARRAY_BUFFER, sizeof(singleVertex), 0, GL_STREAM_DRAW);
            gl::VertexAttribPointer(0, 3, GL_FLOAT,         GL_FALSE, sizeof(singleVertex), u::offset_of(&singleVertex::position)); // position
            gl::VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(singleVertex), u::offset_of(&singleVertex::color)); // color
        }

        gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibos[i]);
        gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof m_indices[0], 0, GL_STREAM_DRAW);
    }

    m_method.enable();
    m_method.setColorTextureUnit(0);
    m_method.setDepthTextureUnit(1);

    m_stats->incTextureCount();
    m_stats->incTextureMemory(m_texture.memory());

    return true;
}

void particleSystem::render(const pipeline &pl) {
    const m::quat rotation = pl.rotation();
    m::vec3 side;
    m::vec3 up;
    rotation.getOrient(nullptr, &up, &side);

    if (gl::has(gl::ARB_half_float_vertex)) {
        m_stats->decVBOMemory(sizeof m_halfVertices[0] * m_halfVertices.size());
        m_halfVertices.destroy();
        m_halfVertices.reserve(m_particles.size() * 4);
    } else {
        m_stats->decVBOMemory(sizeof m_singleVertices[0] * m_singleVertices.size());
        m_singleVertices.destroy();
        m_singleVertices.reserve(m_particles.size() * 4);
    }

    m_stats->decIBOMemory(sizeof m_indices[0] * m_indices.size());

    GLuint &vbo = m_vbos[m_bufferIndex];
    GLuint &ibo = m_ibos[m_bufferIndex];
    m_bufferIndex = (m_bufferIndex + 1) % (sizeof m_vbos / sizeof *m_vbos);

    // Invalidate next buffer a frame in advance to hint the driver that we're
    // doing double-buffering
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[m_bufferIndex]);
    gl::BufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STREAM_DRAW);

    // We use a smaller index format for particles to reduce upload costs
    if (m_particles.size() * 6 > SHRT_MAX) {
        m_particles.resize(SHRT_MAX / 6);
        m_particles.shrink_to_fit();
    }

    m_indices.destroy();
    m_indices.reserve(m_particles.size() * 6);

    // TODO: kick off point in view frustum test for every particle

    // sort particles by ones closest to camera
    u::sort(m_particles.begin(), m_particles.end(),
        [&pl](const particle &lhs, const particle &rhs) {
            const float d1 = (lhs.origin - pl.position()).abs();
            const float d2 = (rhs.origin - pl.position()).abs();
            return d1 > d2;
        }
    );

    for (const auto &it : m_particles) {
        if (!it.visible || it.lifeTime < 0.0f)
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
            const auto &c = m::convertToHalf(&q[0].x, 3*4);
            for (size_t i = 0; i < c.size(); i += 3) {
                halfVertex newVertex;
                for (size_t j = 0; j < 3; j++) {
                    newVertex.color[j] = it.color[j] * 255.0f;
                    newVertex.position[j] = c[i+j];
                }
                newVertex.color[3] = it.alpha * 255.0f;
                m_halfVertices.push_back(newVertex);
            }
        } else {
            index = m_singleVertices.size();
            for (const auto &jt : q) {
                singleVertex newVertex;
                newVertex.position = jt;
                for (size_t i = 0; i < 3; i++)
                    newVertex.color[i] = it.color[i] * 255.0f;
                newVertex.color[3] = it.alpha * 255.0f;
                m_singleVertices.push_back(newVertex);
            }
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

    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        gl::BufferData(GL_ARRAY_BUFFER, m_halfVertices.size() * sizeof(halfVertex), &m_halfVertices[0], GL_STREAM_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, sizeof(halfVertex), u::offset_of(&halfVertex::position)); // position
        gl::VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(halfVertex), u::offset_of(&halfVertex::color)); // color
        m_stats->incVBOMemory(sizeof m_halfVertices[0] * m_halfVertices.size());
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, m_singleVertices.size() * sizeof(singleVertex), &m_singleVertices[0], GL_STREAM_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(singleVertex), u::offset_of(&singleVertex::position)); // position
        gl::VertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(singleVertex), u::offset_of(&singleVertex::color)); // color
        m_stats->incVBOMemory(sizeof m_singleVertices[0] * m_singleVertices.size());
    }

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof m_indices[0],
        &m_indices[0], GL_DYNAMIC_DRAW);
    m_stats->incIBOMemory(sizeof m_indices[0] * m_indices.size());

    m_texture.bind(GL_TEXTURE0);

    m_method.enable();
    m_method.setPerspective(pl.perspective());
    m_method.setVP(pl.projection() * pl.view());
    m_method.setPower(power());

    gl::Disable(GL_CULL_FACE);
    gl::DepthFunc(GL_LESS);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_SHORT, nullptr);

    gl::Enable(GL_CULL_FACE);
}

void particleSystem::addParticle(particle &&p) {
    m_particles.push_back(p);
}

void particleSystem::update(const pipeline &p) {
    const float dt = p.delta() * 0.1f;
    const float g = gravity();

    for (auto &it : m_particles) {
        if (it.lifeTime < 0.0f) {
            if (it.respawn)
                initParticle(it, p.position());
            else
                continue;
        }
        it.origin = it.origin + it.velocity*dt - m::vec3(0.0f, dt*dt*0.5f*g, 0.0f);
        it.velocity.y -= g*dt;
        it.lifeTime -= dt;
        const float f = it.lifeTime / it.totalLifeTime;
        const float scale = m::sin(f * m::kPi);
        it.alpha = it.startAlpha * scale;
        it.size = scale * it.startSize + 0.1f;
    }
}

}
