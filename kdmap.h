#ifndef MAP_HDR
#define MAP_HDR
#include "kdtree.h"

struct kdSphereTrace {
    m::vec3 start;
    m::vec3 direction;
    float radius;
    float fraction;
    m::plane plane;
};

// stack to do iterative depth-first traversals on the tree
struct kdStack {
    kdStack()
        : m_data(nullptr)
        , m_top(-1_z)
        , m_size(0)
    {
    }

    ~kdStack() {
        neoFree(m_data);
    }

    void push(int32_t node) {
        if (m_top == m_size-1)
            resize(m_size * 2);
        m_data[++m_top] = node;
    }

    void reset() {
        m_top = -1_z;
    }

    void resize(size_t size) {
        m_data = (int32_t*)neoRealloc(m_data, sizeof *m_data * size);
        m_size = size;
    }

    int32_t pop() {
        return m_data[m_top--];
    }

    operator bool() const {
        return m_top != -1_z;
    }

private:
    int32_t *m_data;
    size_t m_top;
    size_t m_size;
};

struct kdMap {
    kdMap();
    ~kdMap();

    bool load(const u::vector<unsigned char> &data);
    void unload();

    void traceSphere(kdSphereTrace *trace);
    bool inSphere(u::vector<size_t> &triangleIndices, const m::vec3 &position, float radius);
    bool isSphereStuck(const m::vec3 &position, float radius);

    bool isLoaded() const;

    u::vector<m::plane>      planes;
    u::vector<kdBinTexture>  textures;
    u::vector<kdBinNode>     nodes;
    u::vector<kdBinTriangle> triangles;
    u::vector<kdBinVertex>   vertices;
    u::vector<kdBinEnt>      entities;
    u::vector<kdBinLeaf>     leafs;

    static constexpr float kDistEpsilon = 0.02f; // 2cm epsilon for triangle collisions
    static constexpr float kMinFraction = 0.005f; // no less than 0.5% movement along a direction vector
    static constexpr size_t kMaxClippingPlanes = 5; // maximum number of clipping planes to test
    static constexpr size_t kMaxBumps = 4; // Maximum collision bump iterations
    static constexpr float kFractionScale = 0.95f; // Collision response fractional scale
    static constexpr float kOverClip = 1.01f; // percentage * 100 of overclip allowed in collision detection against planes (lower values == more sticky)
    static constexpr float kStopEpsilon = 0.2f; // minimum velocity size for clipping

    // clips the velocity for collision handling
    static void clipVelocity(const m::vec3 &in, const m::vec3 &normal, m::vec3 &out, float overBounce);

private:
    // sweeping
    bool sphereTriangleIntersect(size_t triangleIndex, const m::vec3 &spherePosition,
        float sphereRadius, const m::vec3 &direction, float *fraction, m::vec3 *hitNormal, m::vec3 *hitPoint) const;

    // static
    bool sphereTriangleIntersectStatic(size_t triangleIndex, const m::vec3 &spherePosition, float sphereRadius) const;

    void traceSphere(kdSphereTrace *trace, int32_t node);
    bool isSphereStuck(const m::vec3 &position, float radius, int32_t node);
    bool inSphere(u::vector<size_t> &triangleIndices, const m::vec3 &position, float radius, int32_t node);

    kdStack m_stack;
};

#endif
