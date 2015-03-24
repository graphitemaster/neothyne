#include "cvar.h"

#include "r_hoq.h"

#include "u_misc.h"

VAR(int, r_maxhoq, "maximum hardware occlusion queries", 1, 32, 8);

namespace r {

bool occlusionMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/hoq.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/hoq.fs"))
        return false;

    if (!finalize({ { 0, "position" } },
                  { { 0, "fragColor" } }))
    {
        return false;
    }

    m_WVPLocation = getUniformLocation("gWVP");

    return true;
}

void occlusionMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

///! occlusionQueries::object
occlusionQueries::object::object(const m::mat4 &wvp, occlusionQueries *owner, ref handle)
    : m_wvp(wvp)
    , m_owner(owner)
    , m_handle(handle)
{
}
void occlusionQueries::object::render() {
    m_owner->m_method.enable();
    m_owner->m_method.setWVP(m_wvp);
    m_owner->m_cube.render();
}

///! occlusionQueries
occlusionQueries::occlusionQueries()
    : m_bits(~0u)
{
}

u::optional<occlusionQueries::ref> occlusionQueries::next() const {
    // Find least-significant bit for the next query
    if (m_bits == 0)
        return u::none;
    for (int i = 0; i < r_maxhoq; i++)
        if (m_bits & (1 << i))
            return i;
    return u::none;
}

bool occlusionQueries::init() {
    if (!m_cube.upload())
        return false;
    if (!m_method.init())
        return false;
    // Force an update, this will generate the appropriate queries (first time around)
    update();
    return true;
}

void occlusionQueries::update() {
    // No need to resize if nothing changes
    if (r_maxhoq == int(m_queries.size()))
        return;

    if (m_queries.size()) {
        // Destroy old queries
        m_objects.clear();
        m_bits = ~0u; // All available
        gl::DeleteQueries(m_queries.size(), &m_queries[0]);
    }

    m_queries.resize(r_maxhoq);
    gl::GenQueries(r_maxhoq, &m_queries[0]);
}

u::optional<occlusionQueries::ref> occlusionQueries::add(const m::mat4 &wvp) {
    const auto index = next();
    if (!index)
        return u::none;
    const auto handle = *index;
    m_objects.push_back({ wvp, this, handle });
    m_bits &= ~(1u << handle); // Object in use
    return handle;
}

bool occlusionQueries::passed(ref handle) {
    if (handle >= m_queries.size())
        return false;

    const auto query = m_queries[handle];
    GLuint available = 0;
    gl::GetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available == 0)
        return false;

    // Available, check the result
    GLuint pass = 0;
    gl::GetQueryObjectuiv(query, GL_QUERY_RESULT, &pass);

    // Make this object available again
    m_bits |= (1u << handle);

    return pass == 0;
}

void occlusionQueries::render() {
    // No color or depth writes for occlusion queries
    gl::ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    gl::DepthMask(GL_FALSE);

    // Execute the occlusion queries
    for (auto &it : m_objects) {
        const auto index = it.m_handle;
        gl::BeginQuery(GL_ANY_SAMPLES_PASSED, m_queries[index]);
        it.render();
        gl::EndQuery(GL_ANY_SAMPLES_PASSED, m_queries[index]);
    }

    m_objects.destroy();

    // Flush the pipeline for this occlusion query pass
    gl::Flush();

    gl::ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl::DepthMask(GL_TRUE);
}

}
