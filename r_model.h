#ifndef R_MODEL_HDR
#define R_MODEL_HDR

#include "u_vector.h"
#include <assert.h>

namespace r {

struct vertexCacheData {
    u::vector<size_t> indices;
    size_t cachePosition;
    float currentScore;
    size_t totalValence;
    size_t remainingValence;
    bool calculated;

    vertexCacheData();

    size_t findTriangle(size_t tri);
    void moveTriangle(size_t tri);
};

struct triangleCacheData {
    bool rendered;
    float currentScore;
    size_t vertices[3];
    bool calculated;

    triangleCacheData();
};

struct vertexCache {
    void addVertex(size_t vertex);
    void clear();
    size_t getCacheMissCount() const;
    size_t getCacheMissCount(const u::vector<size_t> &indices);
    size_t getCachedVertex(size_t index) const;
    vertexCache();
private:
    size_t findVertex(size_t vertex);
    void removeVertex(size_t stackIndex);
    size_t m_cache[40];
    size_t m_misses;
};

struct vertexCacheOptimizer {
    vertexCacheOptimizer();

    enum result {
        kSuccess,
        kErrorInvalidIndex,
        kErrorNoVertices
    };

    result optimize(u::vector<size_t> &indices);

    size_t getCacheMissCount() const;

private:
    u::vector<vertexCacheData> m_vertices;
    u::vector<triangleCacheData> m_triangles;
    u::vector<size_t> m_indices;
    u::vector<size_t> m_drawList;
    vertexCache m_vertexCache;
    size_t m_bestTriangle;

    float calcVertexScore(size_t vertex);
    size_t fullScoreRecalculation();
    result initialPass();
    result init(u::vector<size_t> &indices, size_t vertexCount);
    void addTriangle(size_t triangle);
    bool cleanFlags();
    void triangleScoreRecalculation(size_t triangle);
    size_t partialScoreRecalculation();
    bool process();
};

};

#endif
