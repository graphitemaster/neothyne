#ifndef KDTREE_HDR
#define KDTREE_HDR
#include "math.h"
#include "util.h"

struct kdTree;

struct kdEnt {
    //uint32_t id;
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

    void calculateSphere(const kdTree *tree, const u::vector<int> &triangles);
    bool isLeaf(void) const;
    m::plane findSplittingPlane(const kdTree *tree, const u::vector<int> &triangles, m::axis axis) const;
    void split(const kdTree *tree, const u::vector<int> &triangles, m::axis axis,
        u::vector<int> &front, u::vector<int> &back, u::vector<int> &splitlist, m::plane &splitplane);

    u::vector<unsigned char> serialize(void);

    kdNode        *front;
    kdNode        *back;
    m::plane       splitPlane;   // Not set for leafs
    m::vec3        sphereOrigin;
    float          sphereRadius;
    u::vector<int> triangles;
};

struct kdTree {
    kdTree(void);
    ~kdTree(void);

    static constexpr float kMaxTraceDistance = 99999.999f;
    static constexpr float kEpsilon = 0.01f; // Plane offset for point classification
    static constexpr size_t kMaxTrianglesPerLeaf = 5;
    static constexpr size_t kMaxRecursionDepth = 35;

    bool load(const u::string &file);
    polyPlane testTriangle(size_t index, const m::plane &plane) const;
    void unload(void);
    u::vector<unsigned char> serialize(void);

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
struct kdBinHeader {
    kdBinHeader() :
        magic(kMagic),
        version(kVersion)
    { }

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
    char name[64];
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
    kdBinEnt() :
        origin(0.0f, 0.0f, 0.0f),
        rotation(0.0f, 0.0f, 0.0f, 1.0f)
    { }

    //uint32_t id;
    m::vec3 origin;
    m::quat rotation;
};

struct kdBinLeaf {
    // we treat this as a flexible array member
    u::vector<uint32_t> triangles;
};

#endif
