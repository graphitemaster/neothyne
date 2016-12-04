#ifndef KDTREE_HDR
#define KDTREE_HDR
#include <stdint.h>

#include "u_string.h"
#include "u_vector.h"
#include "u_set.h"

#include "m_plane.h"
#include "m_quat.h"

struct kdTree;

struct kdEnt {
    uint32_t id;
    m::vec3 origin;
    m::quat rotation;
};

struct kdTriangle {
    kdTriangle();
    m::vec3 getNormal(const kdTree *const tree);
    void generatePlane(const kdTree *const tree);

private:
    friend struct kdTree;
    friend struct kdNode;

    int m_vertices[3];
    int m_texCoords[3];
    m::plane m_plane;
    const u::string *m_textureReference;
};

inline kdTriangle::kdTriangle() {
    memset(this, 0, sizeof *this);
}

enum polyPlane : size_t {
    kPolyPlaneSplit,
    kPolyPlaneBack,
    kPolyPlaneFront,
    kPolyPlaneCoplanar
};

struct kdNode {
    kdNode(kdTree *tree, const u::vector<int> &triangles, size_t recursionDepth);
    ~kdNode();

    // Calculate bounding sphere for node
    bool calculateSphere(const kdTree *tree, const u::vector<int> &triangles);

    // Is the node a leaf?
    bool isLeaf() const;

    // Find the best plane to split on
    m::plane findSplittingPlane(const kdTree *tree, const u::vector<int> &triangles, m::axis axis) const;

    // Split triangles along `axis' axis.
    void split(const kdTree *tree, const u::vector<int> &triangles, m::axis axis,
        u::vector<int> &front, u::vector<int> &back, u::vector<int> &splitlist, m::plane &splitplane) const;

    // Flatten tree representation into a disk-writable medium.
    u::vector<unsigned char> serialize();

    kdNode *m_front;
    kdNode *m_back;
    m::plane m_splitPlane;   // Not set for leafs
    m::vec3 m_sphereOrigin;
    float m_sphereRadius;
    u::vector<int> m_triangles;    // Only filled for leaf node
};

struct kdTree {
    kdTree();
    ~kdTree();

    static constexpr float kMaxTraceDistance = 99999.999f;
    static constexpr float kEpsilon = 0.01f; // Plane offset for point classification
    static constexpr size_t kMaxTrianglesPerLeaf = 5;
    static constexpr size_t kMaxRecursionDepth = 35;

    bool load(const u::string &file);
    polyPlane testTriangle(size_t index, const m::plane &plane) const;
    void unload();
    u::vector<unsigned char> serialize();

private:
    friend struct kdNode;
    friend struct kdTriangle;

    kdNode *m_root;
    u::vector<m::vec3> m_vertices;
    u::vector<m::vec2> m_texCoords;
    u::vector<kdTriangle> m_triangles;
    u::vector<kdEnt> m_entities;
    u::set<u::string> m_textures;
    size_t m_nodeCount;
    size_t m_leafCount;
    size_t m_textureCount;
    size_t m_depth;
};

// Serialized version for storing on disk
#pragma pack(push, 1)
struct kdBinHeader {
    kdBinHeader()
        : magic(kMagic)
        , version(kVersion)
        , padding(0)
    {
    }

    enum : uint32_t {
        kMagic   = 0x66551133,
        kVersion = 1
    };

    uint32_t magic;
    uint32_t version;
    uint8_t padding;

    void endianSwap();
};

struct kdBinEntry {
    uint32_t offset;
    uint32_t length;

    void endianSwap();
};

struct kdBinPlane {
    uint8_t type;
    float d;

    void endianSwap();
};

struct kdBinTexture {
    char name[255];
};

struct kdBinNode {
    uint32_t plane;
    int32_t children[2]; // Leaf indices are stored with negitive index
    float sphereRadius;
    m::vec3 sphereOrigin;

    void endianSwap();
};

struct kdBinTriangle {
    uint32_t texture;
    uint32_t v[3];

    void endianSwap();
};

struct kdBinVertex {
    // GPU layout:
    // P.X  P.Y  P.Z  N.X
    // N.Y  N.Z  C.X  C.Y
    // T.X  T.Y  T.Z  T.W
    m::vec3 vertex;
    m::vec3 normal;
    m::vec2 coordinate;
    m::vec4 tangent;
    void endianSwap();
};

struct kdBinEnt {
    kdBinEnt()
        : id(0)
        , rotation(0.0f, 0.0f, 0.0f, 1.0f)
    {
    }

    uint32_t id;
    m::vec3 origin;
    m::quat rotation;

    void endianSwap();
};
#pragma pack(pop)

struct kdBinLeaf {
    // we treat this as a flexible array member
    u::vector<uint32_t> triangles;
};

#endif
