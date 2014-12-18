#ifndef MESH_HDR
#define MESH_HDR
#include "u_vector.h"

#include "m_bbox.h"

namespace u {
    struct string;
}

struct obj {
    bool load(const u::string &file);

    u::vector<size_t> indices() const;
    u::vector<m::vec3> positions() const;
    u::vector<m::vec3> normals() const;
    u::vector<m::vec3> coordinates() const;

private:
    friend struct mesh;

    u::vector<size_t> m_indices;
    u::vector<m::vec3> m_positions;
    u::vector<m::vec3> m_normals;
    u::vector<m::vec3> m_coordinates;
};

struct mesh {
    bool load(const u::string &file);

    u::vector<size_t> indices() const;
    u::vector<m::vec3> positions() const;
    u::vector<m::vec3> normals() const;
    u::vector<m::vec3> coordinates() const;

    m::bbox bbox;

private:
    obj m_obj; // TO BE EXTENDED
};

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

struct face {
    face();
    size_t vertex;
    size_t normal;
    size_t coordinate;
};

inline face::face()
    : vertex((size_t)-1)
    , normal((size_t)-1)
    , coordinate((size_t)-1)
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
    static constexpr size_t prime1 = 73856093u;
    static constexpr size_t prime2 = 19349663u;
    static constexpr size_t prime3 = 83492791u;
    return (f.vertex * prime1) ^ (f.normal * prime2) ^ (f.coordinate * prime3);
}

}

#endif
