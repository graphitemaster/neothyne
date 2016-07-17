#ifndef MESH_HDR
#define MESH_HDR
#include "u_vector.h"
#include "u_misc.h"

#include "m_bbox.h"
#include "m_half.h"

namespace u {
    struct string;
}

struct Model;

namespace Mesh {
    struct GeneralVertex {
        float position[3];
        float normal[3];
        float coordinate[2];
        float tangent[4]; // w = sign of bitangent
    };

    struct AnimVertex : GeneralVertex {
        unsigned char blendIndex[4];
        unsigned char blendWeight[4];
    };

    struct GeneralHalfVertex {
        m::half position[3];
        m::half normal[3];
        m::half coordinate[2];
        m::half tangent[4]; // w = sign of bitangent
    };

    struct AnimHalfVertex : GeneralHalfVertex {
        unsigned char blendIndex[4];
        unsigned char blendWeight[4];
    };
}

struct VertexCacheData {
    u::vector<size_t> indices;
    size_t cachePosition;
    float currentScore;
    size_t totalValence;
    size_t remainingValence;
    bool calculated;

    VertexCacheData();

    size_t findTriangle(size_t tri);
    void moveTriangle(size_t tri);
};

struct TriangleCacheData {
    bool rendered;
    float currentScore;
    size_t vertices[3];
    bool calculated;

    TriangleCacheData();
};

struct VertexCache {
    void addVertex(size_t vertex);
    void clear();
    size_t getCacheMissCount() const;
    size_t getCacheMissCount(const u::vector<size_t> &indices);
    size_t getCachedVertex(size_t index) const;
    VertexCache();
private:
    size_t findVertex(size_t vertex);
    void removeVertex(size_t stackIndex);
    size_t m_cache[40];
    size_t m_misses;
};

struct VertexCacheOptimizer {
    VertexCacheOptimizer();

    enum Result {
        kSuccess,
        kErrorInvalidIndex,
        kErrorNoVertices
    };

    Result optimize(u::vector<size_t> &indices);

    size_t getCacheMissBefore() const;
    size_t getCacheMissAfter() const;

private:
    u::vector<VertexCacheData> m_vertices;
    u::vector<TriangleCacheData> m_triangles;

    u::vector<size_t> m_indices;
    u::vector<size_t> m_drawList;
    VertexCache m_vertexCache;
    size_t m_bestTriangle;
    size_t m_cacheMissesBefore;
    size_t m_cacheMissesAfter;

    float calcVertexScore(size_t vertex);
    size_t fullScoreRecalculation();
    Result initialPass();
    Result init(u::vector<size_t> &indices, size_t vertexCount);
    void addTriangle(size_t triangle);
    bool cleanFlags();
    void triangleScoreRecalculation(size_t triangle);
    size_t partialScoreRecalculation();
    bool process();
};

inline size_t VertexCacheOptimizer::getCacheMissBefore() const {
    return m_cacheMissesBefore;
}

inline size_t VertexCacheOptimizer::getCacheMissAfter() const {
    return m_cacheMissesAfter;
}


struct Face {
    Face();
    size_t vertex;
    size_t normal;
    size_t coordinate;
};

inline Face::Face()
    : vertex(-1_z)
    , normal(-1_z)
    , coordinate(-1_z)
{
}

inline bool operator==(const Face &lhs, const Face &rhs) {
    return lhs.vertex == rhs.vertex
        && lhs.normal == rhs.normal
        && lhs.coordinate == rhs.coordinate;
}

// Hash a face
namespace u {

inline size_t hash(const ::Face &f) {
    static constexpr size_t kPrime1 = 73856093_z;
    static constexpr size_t kPrime2 = 19349663_z;
    static constexpr size_t kPrime3 = 83492791_z;
    return (f.vertex * kPrime1) ^ (f.normal * kPrime2) ^ (f.coordinate * kPrime3);
}

}

#endif
