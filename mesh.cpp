#include "mesh.h"
#include "engine.h"

#include "u_file.h"
#include "u_map.h"

#include "m_vec.h"
#include "m_trig.h"

///! vertexCacheData
size_t vertexCacheData::findTriangle(size_t triangle) {
    for (size_t i = 0; i < indices.size(); i++)
        if (indices[i] == triangle)
            return i;
    return -1_z;
}

void vertexCacheData::moveTriangle(size_t triangle) {
    const size_t index = findTriangle(triangle);
    U_ASSERT(index != -1_z);

    // Erase the index and add it to the end
    indices.erase(indices.begin() + index,
                  indices.begin() + index + 1);
    indices.push_back(triangle);
}

vertexCacheData::vertexCacheData()
    : cachePosition(-1_z)
    , currentScore(0.0f)
    , totalValence(0)
    , remainingValence(0)
    , calculated(false)
{
}

///!triangleCacheData
triangleCacheData::triangleCacheData()
    : rendered(false)
    , currentScore(0.0f)
    , calculated(false)
{
    for (size_t i = 0; i < 3; i++)
        vertices[i] = -1_z;
}

///! vertexCache
size_t vertexCache::findVertex(size_t vertex) {
    for (size_t i = 0; i < 32; i++)
        if (m_cache[i] == vertex)
            return i;
    return -1_z;
}

void vertexCache::removeVertex(size_t stackIndex) {
    for (size_t i = stackIndex; i < 38; i++)
        m_cache[i] = m_cache[i + 1];
}

void vertexCache::addVertex(size_t vertex) {
    const size_t w = findVertex(vertex);
    // remove the vertex for later reinsertion at the top
    if (w != -1_z)
        removeVertex(w);
    else // not found, cache miss!
        m_misses++;
    // shift all vertices down to make room for the new top vertex
    for (size_t i = 39; i != 0; i--)
        m_cache[i] = m_cache[i - 1];
    // add new vertex to cache
    m_cache[0] = vertex;
}

void vertexCache::clear() {
    for (size_t i = 0; i < 40; i++)
        m_cache[i] = -1_z;
    m_misses = 0;
}

size_t vertexCache::getCacheMissCount() const {
    return m_misses;
}

size_t vertexCache::getCacheMissCount(const u::vector<size_t> &indices) {
    clear();
    for (const auto &it : indices)
        addVertex(it);
    return m_misses;
}

size_t vertexCache::getCachedVertex(size_t index) const {
    return m_cache[index];
}

vertexCache::vertexCache() {
    clear();
}

///! vertexCacheOptimizer
static constexpr float kCacheDecayPower = 1.5f;
static constexpr float kLastTriScore = 0.75f;
static constexpr float kValenceBoostScale = 2.0f;
static constexpr float kValenceBoostPower = 0.5f;

vertexCacheOptimizer::vertexCacheOptimizer()
    : m_bestTriangle(0)
    , m_cacheMissesBefore(0)
    , m_cacheMissesAfter(0)
{
}

vertexCacheOptimizer::result vertexCacheOptimizer::optimize(u::vector<size_t> &indices) {
    // Calculate the initial vertex cache misses
    vertexCache countMisses;
    m_cacheMissesBefore = countMisses.getCacheMissCount(indices);

    size_t find = -1_z;
    for (size_t i = 0; i < indices.size(); i++)
        if (find == -1_z || (find != -1_z && indices[i] > find))
            find = indices[i];
    if (find == -1_z)
        return kErrorNoVertices;

    const result begin = init(indices, find + 1);
    if (begin != kSuccess)
        return begin;

    // Process
    while (process())
        ;

    m_cacheMissesAfter = m_vertexCache.getCacheMissCount();

    // Rewrite the indices
    for (size_t i = 0; i < m_drawList.size(); i++)
        for (size_t j = 0; j < 3; j++)
            indices[3 * i + j] = m_triangles[m_drawList[i]].vertices[j];

    return kSuccess;
}

float vertexCacheOptimizer::calcVertexScore(size_t vertex) {
    const vertexCacheData *const v = &m_vertices[vertex];
    if (v->remainingValence == -1_z || v->remainingValence == 0)
        return -1.0f; // No triangle needs this vertex

    float value = 0.0f;
    if (v->cachePosition == -1_z) {
        // Vertex is not in FIFO cache.
    } else {
        if (v->cachePosition < 3) {
            // This vertex was used in the last triangle. It has fixed score
            // in whichever of the tree it's in.
            value = kLastTriScore;
        } else {
            // Points for being heigh in the cache
            const float scale = 1.0f / (32 - 3);
            value = 1.0f - (v->cachePosition - 3) * scale;
            value = m::pow(value, kCacheDecayPower);
        }
    }

    // Bonus points for having a low number of triangles.
    const float valenceBoost = m::pow(float(v->remainingValence), -kValenceBoostPower);
    value += kValenceBoostScale * valenceBoost;
    return value;
}

size_t vertexCacheOptimizer::fullScoreRecalculation() {
    // Calculate score for all vertices
    for (size_t i = 0; i < m_vertices.size(); i++)
        m_vertices[i].currentScore = calcVertexScore(i);

    // Calculate scores for all active triangles
    float maxScore = 0.0f;
    size_t maxScoreTriangle = -1_z;
    bool firstTime = true;

    for (size_t i = 0; i < m_triangles.size(); i++) {
        auto &it = m_triangles[i];
        if (it.rendered)
            continue;

        // Sum the score of all the triangle's vertices
        const float sum = m_vertices[it.vertices[0]].currentScore +
                          m_vertices[it.vertices[1]].currentScore +
                          m_vertices[it.vertices[2]].currentScore;
        it.currentScore = sum;

        if (firstTime || sum > maxScore) {
            firstTime = false;
            maxScore = sum;
            maxScoreTriangle = i;
        }
    }

    return maxScoreTriangle;
}

vertexCacheOptimizer::result vertexCacheOptimizer::initialPass() {
    for (size_t i = 0; i < m_indices.size(); i++) {
        const size_t index = m_indices[i];
        if (index == -1_z || index >= m_vertices.size())
            return kErrorInvalidIndex;
        m_vertices[index].totalValence++;
        m_vertices[index].remainingValence++;
        m_vertices[index].indices.push_back(i / 3);
    }
    m_bestTriangle = fullScoreRecalculation();
    return kSuccess;
}

vertexCacheOptimizer::result vertexCacheOptimizer::init(u::vector<size_t> &indices, size_t maxVertex) {
    const size_t triangleCount = indices.size() / 3;

    // Reset draw list
    m_drawList.clear();
    m_drawList.reserve(maxVertex);

    // Reset and initialize vertices and triangles
    m_vertices.clear();
    m_vertices.reserve(maxVertex);
    for (size_t i = 0; i < maxVertex; i++)
        m_vertices.push_back(vertexCacheData());

    m_triangles.clear();
    m_triangles.reserve(triangleCount);
    for (size_t i = 0; i < triangleCount; i++) {
        triangleCacheData data;
        for (size_t j = 0; j < 3; j++)
            data.vertices[j] = indices[i * 3 + j];
        m_triangles.push_back(data);
    }

    // Copy the indices
    m_indices.clear();
    m_indices.reserve(indices.size());
    for (const auto &it : indices)
        m_indices.push_back(it);

    // Run the initial pass
    m_vertexCache.clear();
    m_bestTriangle = -1_z;

    return initialPass();
}

void vertexCacheOptimizer::addTriangle(size_t triangle) {
    // reset all cache positions
    for (size_t i = 0; i < 32; i++) {
        const size_t find = m_vertexCache.getCachedVertex(i);
        if (find == -1_z)
            continue;
        m_vertices[find].cachePosition = -1_z;
    }

    triangleCacheData *const t = &m_triangles[triangle];
    if (t->rendered)
        return;

    for (size_t i = 0; i < 3; i++) {
        // Add all the triangle's vertices to the cache
        m_vertexCache.addVertex(t->vertices[i]);
        vertexCacheData *const v = &m_vertices[t->vertices[i]];

        // Decrease the remaining valence.
        v->remainingValence--;

        // Move the added triangle to the end of the vertex's triangle index
        // list such that the first `remainingValence' triangles in the index
        // list are only the active ones.
        v->moveTriangle(triangle);
    }

    // It's been rendered, mark it
    m_drawList.push_back(triangle);
    t->rendered = true;

    // Update all the vertex cache positions
    for (size_t i = 0; i < 32; i++) {
        const size_t index = m_vertexCache.getCachedVertex(i);
        if (index == -1_z)
            continue;
        m_vertices[index].cachePosition = i;
    }
}

// Avoid duplicate calculations during processing. Triangles and vertices have
// a `calculated' flag which must be reset at the beginning of the process for
// all active triangles that have one or more of their vertices currently in
// cache as well all their other vertices.
//
// If there aren't any active triangles in the cache this function returns
// false and a full recalculation of the tree is performed.
bool vertexCacheOptimizer::cleanFlags() {
    bool found = false;
    for (size_t i = 0; i < 32; i++) {
        const size_t find = m_vertexCache.getCachedVertex(i);
        if (find == -1_z)
            continue;

        const vertexCacheData *const v = &m_vertices[find];
        for (size_t j = 0; j < v->remainingValence; j++) {
            triangleCacheData *const t = &m_triangles[v->indices[j]];
            found = true;
            // Clear flags
            t->calculated = false;
            for (size_t k = 0; k < 3; k++)
                m_vertices[t->vertices[k]].calculated = false;
        }
    }
    return found;
}

void vertexCacheOptimizer::triangleScoreRecalculation(size_t triangle) {
    triangleCacheData *const t = &m_triangles[triangle];

    // Calculate vertex scores
    float sum = 0.0f;
    for (size_t i = 0; i < 3; i++) {
        vertexCacheData *const v = &m_vertices[t->vertices[i]];
        const float score = v->calculated ? v->currentScore : calcVertexScore(t->vertices[i]);
        v->currentScore = score;
        v->calculated = true;
        sum += score;
    }

    t->currentScore = sum;
    t->calculated = true;
}

size_t vertexCacheOptimizer::partialScoreRecalculation() {
    // Iterate through all the vertices of the cache
    bool firstTime = true;
    float maxScore = 0.0f;
    size_t maxScoreTriangle = size_t(-1);
    for (size_t i = 0; i < 32; i++) {
        const size_t find = m_vertexCache.getCachedVertex(i);
        if (find == -1_z)
            continue;

        const vertexCacheData *const v = &m_vertices[find];

        // Iterate through all the active triangles of this vertex
        for (size_t j = 0; j < v->remainingValence; j++) {
            const size_t triangle = v->indices[j];
            const triangleCacheData *const t = &m_triangles[triangle];

            // Calculate triangle score if it isn't already calculated
            if (!t->calculated)
                triangleScoreRecalculation(triangle);

            const float score = t->currentScore;
            // Found a triangle to process
            if (firstTime || score > maxScore) {
                firstTime = false;
                maxScore = score;
                maxScoreTriangle = triangle;
            }
        }
    }
    return maxScoreTriangle;
}

inline bool vertexCacheOptimizer::process() {
    if (m_drawList.size() == m_triangles.size())
        return false;

    // Add the selected triangle to the draw list
    addTriangle(m_bestTriangle);

    // Recalculate the vertex and triangle scores and select the best triangle
    // for the next iteration.
    m_bestTriangle = cleanFlags() ? partialScoreRecalculation() : fullScoreRecalculation();

    return true;
}
