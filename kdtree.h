#ifndef KDTREE_HDR
#define KDTREE_HDR
#include <stdint.h>

#include "u_string.h"
#include "u_vector.h"

#include "m_plane.h"
#include "m_quat.h"

struct kdTree;

struct kdEnt {
    uint32_t id;
    m::vec3 origin;
    m::quat rotation;
};

struct kdTriangle {
    m::vec3 getNormal(const kdTree *const tree);
    void generatePlane(const kdTree *const tree);

private:
    friend struct kdTree;
    friend struct kdNode;

    int vertices[3];
    int texCoords[3];
    m::plane plane;
    u::string texturePath;
};

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
    void calculateSphere(const kdTree *tree, const u::vector<int> &triangles);

    // Is the node a leaf?
    bool isLeaf() const;

    // Find the best plane to split on
    m::plane findSplittingPlane(const kdTree *tree, const u::vector<int> &triangles, m::axis axis) const;

    // Split triangles along `axis' axis.
    void split(const kdTree *tree, const u::vector<int> &triangles, m::axis axis,
        u::vector<int> &front, u::vector<int> &back, u::vector<int> &splitlist, m::plane &splitplane) const;

    // Flatten tree representation into a disk-writable medium.
    u::vector<unsigned char> serialize();

    kdNode        *front;
    kdNode        *back;
    m::plane       splitPlane;   // Not set for leafs
    m::vec3        sphereOrigin;
    float          sphereRadius;
    u::vector<int> triangles;    // Only filled for leaf node
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

    kdNode                 *root;
    u::vector<m::vec3>      vertices;
    u::vector<m::vec3>      texCoords;
    u::vector<kdTriangle>   triangles;
    u::vector<kdEnt>        entities;
    size_t                  nodeCount;
    size_t                  leafCount;
    size_t                  textureCount;
    size_t                  depth;
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
};

struct kdBinEntry {
    uint32_t offset;
    uint32_t length;
};

struct kdBinPlane {
    uint8_t type;
    float d;
};

struct kdBinTexture {
    char name[255];
};

struct kdBinNode {
    uint32_t plane;
    int32_t children[2]; // Leaf indices are stored with negitive index
    float sphereRadius;
    m::vec3 sphereOrigin;
};

struct kdBinTriangle {
    uint32_t texture;
    uint32_t v[3];
};

struct kdBinVertex {
    m::vec3 vertex;
    m::vec3 normal;
    float tu;
    float tv;
    m::vec3 tangent;
    float w; // bitangent = w * (normal x tangent)
    float padding[2];
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
};
#pragma pack(pop)

struct kdBinLeaf {
    // we treat this as a flexible array member
    u::vector<uint32_t> triangles;
};

#endif
