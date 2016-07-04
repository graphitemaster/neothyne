#ifndef MESH_HDR
#define MESH_HDR
#include "u_vector.h"
#include "u_misc.h"

#include "m_bbox.h"
#include "m_half.h"

namespace u {
    struct string;
}

struct model;
struct obj;

namespace mesh {
    struct basicVertex {
        float position[3];
        float normal[3];
        float coordinate[2];
        float tangent[4]; // w = sign of bitangent
    };

    struct animVertex : basicVertex {
        unsigned char blendIndex[4];
        unsigned char blendWeight[4];
    };

    struct basicHalfVertex {
        m::half position[3];
        m::half normal[3];
        m::half coordinate[2];
        m::half tangent[4]; // w = sign of bitangent
    };

    struct animHalfVertex : basicHalfVertex {
        unsigned char blendIndex[4];
        unsigned char blendWeight[4];
    };
}

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

    size_t getCacheMissBefore() const;
    size_t getCacheMissAfter() const;

private:
    u::vector<vertexCacheData> m_vertices;
    u::vector<triangleCacheData> m_triangles;

    u::vector<size_t> m_indices;
    u::vector<size_t> m_drawList;
    vertexCache m_vertexCache;
    size_t m_bestTriangle;
    size_t m_cacheMissesBefore;
    size_t m_cacheMissesAfter;

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

inline size_t vertexCacheOptimizer::getCacheMissBefore() const {
    return m_cacheMissesBefore;
}

inline size_t vertexCacheOptimizer::getCacheMissAfter() const {
    return m_cacheMissesAfter;
}


struct face {
    face();
    size_t vertex;
    size_t normal;
    size_t coordinate;
};

inline face::face()
    : vertex(-1_z)
    , normal(-1_z)
    , coordinate(-1_z)
{
}

inline bool operator==(const face &lhs, const face &rhs) {
    return lhs.vertex == rhs.vertex
        && lhs.normal == rhs.normal
        && lhs.coordinate == rhs.coordinate;
}

// Hash a face
namespace u {

inline size_t hash(const ::face &f) {
    static constexpr size_t kPrime1 = 73856093_z;
    static constexpr size_t kPrime2 = 19349663_z;
    static constexpr size_t kPrime3 = 83492791_z;
    return (f.vertex * kPrime1) ^ (f.normal * kPrime2) ^ (f.coordinate * kPrime3);
}

}

#endif
