#include "cvar.h"

#include "r_hoq.h"

#include "u_misc.h"

VAR(int, r_maxhoq, "maximum hardware occlusion queries", 1, 8, 16);

namespace r {

bool occlusionMethod::init() {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/hoq.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/hoq.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");

    return true;
}

void occlusionMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

///! occlusionQueries::object
occlusionQueries::object::object(const m::mat4 &wvp, occlusionQueries *owner)
    : m_wvp(wvp)
    , m_owner(owner)
    , m_index(size_t(-1))
{
}

occlusionQueries::object &occlusionQueries::object::operator=(const occlusionQueries::object &other) {
    m_wvp = other.m_wvp;
    m_owner = other.m_owner;
    m_index = other.m_index;
    return *this;
}

void occlusionQueries::object::render(size_t index) {
    m_owner->m_method.enable();
    m_owner->m_method.setWVP(m_wvp);
    m_owner->m_cube.render();
    m_index = index; // Save the query index
}

///! occlusionQueries
occlusionQueries::occlusionQueries()
    : m_next(0)
    , m_current(0)
{
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

    // Wait for all the queries to be complete when resetting
    if (m_queries.size()) {
        size_t count = 0;
        while (count != m_queries.size()) {
            for (auto &it : m_queries) {
                GLuint available = 0;
                gl::GetQueryObjectuiv(it, GL_QUERY_RESULT_AVAILABLE, &available);
                if (available)
                    count++;
            }
        }

        // Destroy old queries
        gl::DeleteQueries(m_queries.size(), &m_queries[0]);
    }

    // Allocate query objects up front
    m_queries.resize(r_maxhoq);
    gl::GenQueries(r_maxhoq, &m_queries[0]);

    m_next = 0;
    m_current = 0;
}

void occlusionQueries::add(const m::mat4 &wvp, const void *id) {
    m_objects.push_back({ wvp, this });
    m_mapping.insert({ id, m_objects.size() });
}

bool occlusionQueries::passed(const void *id) {
    // Is there a mapping for this id?
    auto find = m_mapping.find(id);
    if (find == m_mapping.end())
        return false;

    // Get the index from the mapping
    const size_t index = m_mapping[id];
    if (index >= m_objects.size())
        return false;
    // Get query from the object
    const auto &what = m_objects[index];
    if (what.m_index >= m_queries.size())
        return false;
    // Get the result
    const auto query = m_queries[what.m_index];
    GLuint available = 0;
    gl::GetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
    if (available == 0)
        return false;

    // Available, check the result
    GLuint pass = 0;
    gl::GetQueryObjectuiv(query, GL_QUERY_RESULT, &pass);

    // Remove the object
    m_mapping.erase(find);
    m_objects.erase(m_objects.begin() + index);

    return pass == 0;
}

bool occlusionQueries::begin() {
    // Linear probe for next available query object
    m_next = -1;
    for (auto &it : m_queries) {
        // Make sure it's not in use by some mapping object as well
        const size_t next = &it - &m_queries[0];
        bool inUse = false;
        for (auto &of : m_mapping) {
            if (m_objects[of.second].m_index != next)
                continue;
            inUse = true;
            break;
        }
        // Query object is in use, ignore issuing anymore
        if (inUse)
            return false;

        // It's not in use, own it now.
        m_next = next;
        break;
    }
    // No query this time
    if (m_next == -1)
        return false;

    m_current = m_next;
    gl::BeginQuery(GL_ANY_SAMPLES_PASSED, m_queries[m_current]);
    return true;
}

void occlusionQueries::render() {
    // No color or depth writes for occlusion queries
    gl::ColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    gl::DepthMask(GL_FALSE);

    // Execute the occlusion queries
    for (auto &it : m_objects) {
        // Don't issue an occlusion query for an object that already has one.
        if (it.m_index != size_t(-1))
            continue;
        // Can't acquire, ignore occlusion queries for all objects this frame
        if (!begin())
            break;
        // Render bounding box for occlusion query
        it.render(m_current);
        gl::EndQuery(GL_ANY_SAMPLES_PASSED, m_queries[m_current]);
    }

    // Flush the pipeline for this occlusion query pass
    gl::Flush();
}

}
