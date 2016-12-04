#include <string.h>

#include "kdtree.h"

#include "u_file.h"
#include "u_algorithm.h"
#include "u_misc.h"
#include "u_zlib.h"
#include "u_log.h"

///! triangle
m::vec3 kdTriangle::getNormal(const kdTree *const tree) {
    if (m_plane.n.isNull())
        generatePlane(tree);
    return m_plane.n;
}

void kdTriangle::generatePlane(const kdTree *const tree) {
    m_plane = { tree->m_vertices[m_vertices[0]],
                tree->m_vertices[m_vertices[1]],
                tree->m_vertices[m_vertices[2]] };
}

///! node
kdNode::~kdNode() {
    delete m_front;
    delete m_back;
}

kdNode::kdNode(kdTree *tree, const u::vector<int> &tris, size_t recursionDepth)
    : m_front(nullptr)
    , m_back(nullptr)
    , m_sphereRadius(0.0f)
{
    const size_t triangleCount = tris.size();

    if (recursionDepth > tree->m_depth)
        tree->m_depth = recursionDepth;
    if (recursionDepth > kdTree::kMaxRecursionDepth)
        return;

    tree->m_nodeCount++;

    if (!calculateSphere(tree, tris)) {
        u::Log::err("[world] => level geometry is too large: collision detection and rendering may not work");
        return;
    }

    u::vector<int> fx, fy, fz; // front
    u::vector<int> bx, by, bz; // back
    u::vector<int> sx, sy, sz; // split
    u::vector<int> *const frontList[3] = { &fx, &fy, &fz };
    u::vector<int> *const backList[3] = { &bx, &by, &bz };
    u::vector<int> *const splitList[3] = { &sx, &sy, &sz };

    m::plane plane[3];
    float ratio[3];
    size_t best = recursionDepth % 3;

    // find a plane which gives a good balanced node
    for (size_t i = 0; i < 3; i++) {
        split(tree, tris, m::axis(i), *frontList[i], *backList[i], *splitList[i], plane[i]);
        const size_t fsize = (*frontList[i]).size();
        const size_t bsize = (*backList[i]).size();
        if (fsize > bsize)
            ratio[i] = (float)bsize / (float)fsize;
        else
            ratio[i] = (float)fsize / (float)bsize;
    }

    float bestRatio = 0.0f;
    for (size_t i = 0; i < 3; i++) {
        if (ratio[i] > bestRatio) {
            best = i;
            bestRatio = ratio[i];
        }
    }

    m_splitPlane = plane[best];

    // when there isn't many triangles left, create a leaf. In doing so we can
    // continue to create further subdivisions.
    if (frontList[best]->size() == 0 || backList[best]->size() == 0 || triangleCount <= kdTree::kMaxTrianglesPerLeaf) {
        // create subspace with `triangleCount` polygons
        m_triangles.insert(m_triangles.begin(), tris.begin(), tris.end());
        tree->m_leafCount++;
        return;
    }

    // insert the split triangles on both sides of the plane.
    frontList[best]->insert(frontList[best]->end(), splitList[best]->begin(), splitList[best]->end());
    backList[best]->insert(backList[best]->end(), splitList[best]->begin(), splitList[best]->end());

    // recurse
    m_front = new kdNode(tree, *frontList[best], recursionDepth + 1);
    m_back = new kdNode(tree, *backList[best], recursionDepth + 1);
}

bool kdNode::isLeaf() const {
    return !m_front && !m_back;
}

void kdNode::split(const kdTree *tree, const u::vector<int> &tris, m::axis axis,
    u::vector<int> &frontList, u::vector<int> &backList, u::vector<int> &splitList, m::plane &plane) const
{
    const size_t triangleCount = tris.size();
    plane = findSplittingPlane(tree, tris, axis);
    for (size_t i = 0; i < triangleCount; i++) {
        switch (tree->testTriangle(tris[i], plane)) {
        case kPolyPlaneCoplanar:
        case kPolyPlaneSplit:
            splitList.push_back(tris[i]);
            break;
        case kPolyPlaneFront:
            frontList.push_back(tris[i]);
            break;
        case kPolyPlaneBack:
            backList.push_back(tris[i]);
            break;
        }
    }
}

m::plane kdNode::findSplittingPlane(const kdTree *tree, const u::vector<int> &tris, m::axis axis) const {
    const size_t triangleCount = tris.size();
    // every vertex component is stored depending on `axis' axis in the following
    // vector. The vector gets sorted and the median is chosen as the splitting
    // plane.
    u::vector<float> coords(triangleCount * 3); // 3 vertices for a triangle

    size_t k = 0;
    for (size_t i = 0; i < triangleCount; i++) {
        for (size_t j = 0; j < 3; j++) {
            const int index = tree->m_triangles[tris[i]].m_vertices[j];
            const m::vec3 &vec = tree->m_vertices[index];
            coords[k++] = vec[axis];
        }
    }

    // sort coordinates for a L1 median estimation, this keeps us rather
    // robust against vertex outliers.
    u::sort(coords.begin(), coords.end(), [](float a, float b) { return a < b; });
    const float split = coords[coords.size() / 2]; // median like
    const m::vec3 point(m::vec3::getAxis(axis) * split);
    const m::vec3 normal(m::vec3::getAxis(axis));
    return m::plane(point, normal);
}

bool kdNode::calculateSphere(const kdTree *tree, const u::vector<int> &tris) {
    const size_t triangleCount = tris.size();
    m::vec3 min;
    m::vec3 max;
    for (size_t i = 0; i < triangleCount; i++) {
        const int index = tris[i];
        for (size_t j = 0; j < 3; j++) {
            const size_t vertexIndex = tree->m_triangles[index].m_vertices[j];
            const m::vec3 *v = &tree->m_vertices[vertexIndex];
            if (v->x < min.x) min.x = v->x;
            if (v->y < min.y) min.y = v->y;
            if (v->z < min.z) min.z = v->z;
            if (v->x > max.x) max.x = v->x;
            if (v->y > max.y) max.y = v->y;
            if (v->z > max.z) max.z = v->z;
        }
    }
    const m::vec3 mid = (max - min) * 0.5f;
    m_sphereOrigin = min + mid;
    m_sphereRadius = mid.abs();

    return m_sphereRadius <= kdTree::kMaxTraceDistance;
}

void kdBinHeader::endianSwap() {
    magic = u::endianSwap(magic);
    version = u::endianSwap(version);
    // padding is a byte
}

void kdBinEntry::endianSwap() {
    offset = u::endianSwap(offset);
    length = u::endianSwap(length);
}

void kdBinPlane::endianSwap() {
    // type is a byte
    d = u::endianSwap(d);
}

void kdBinNode::endianSwap() {
    plane = u::endianSwap(plane);
    children[0] = u::endianSwap(children[0]);
    children[1] = u::endianSwap(children[1]);
    sphereRadius = u::endianSwap(sphereRadius);
    sphereOrigin.endianSwap();
}

void kdBinTriangle::endianSwap() {
    texture = u::endianSwap(texture);
    v[0] = u::endianSwap(v[0]);
    v[1] = u::endianSwap(v[1]);
    v[2] = u::endianSwap(v[2]);
}

void kdBinVertex::endianSwap() {
    vertex.endianSwap();
    normal.endianSwap();
    tangent.endianSwap();
    coordinate.endianSwap();
}

void kdBinEnt::endianSwap() {
    id = u::endianSwap(id);
    origin.endianSwap();
    rotation.endianSwap();
}

///! kdTree
kdTree::kdTree()
    : m_root(nullptr)
    , m_nodeCount(0)
    , m_leafCount(0)
    , m_textureCount(0)
    , m_depth(0)
{
}

kdTree::~kdTree() {
    unload();
}

void kdTree::unload() {
    delete m_root;
    m_root = nullptr;
    m_entities.destroy();
    m_vertices.destroy();
    m_texCoords.destroy();
    m_triangles.destroy();
    m_nodeCount = 0;
    m_leafCount = 0;
    m_textureCount = 0;
    m_depth = 0;
}

polyPlane kdTree::testTriangle(size_t index, const m::plane &plane) const {
    int frontBits = kPolyPlaneFront;
    int backBits = kPolyPlaneBack;
    const int *indexTo = m_triangles[index].m_vertices; // indices to vertices for triangle
    for (size_t i = 0; i < 3; i++) {
        switch (plane.classify(m_vertices[indexTo[i]], kEpsilon)) {
        case m::plane::kFront:
            backBits = kPolyPlaneSplit;
            break;
        case m::plane::kBack:
            frontBits = kPolyPlaneSplit;
            break;
        default:
            break;
        }
    }
    return (polyPlane)(frontBits | backBits);
}

bool kdTree::load(const u::string &file) {
    unload();

    auto fp = u::fopen(file, "rt");
    if (!fp.get())
        return false;

    // This is a minimal OBJ loader which makes assumptions about the format.
    // Specifically it assumes the format looks like that of a Cube2 exported
    // map. It expects "g slot%d" for texture slots. It expects the vertex data
    // is already in triangulated format as well.
    const u::string *textureReference = nullptr;
    for (u::string line; u::getline(fp, line); ) {
        float x0, y0, z0, x1, y1, z1, w;
        int v0, v1, v2, t0, t1, t2, i;
        int s0, s1, s2;
        int slot;
        if (u::sscanf(line, "v %f %f %f", &x0, &y0, &z0) == 3) {
            m_vertices.push_back({ x0, y0, z0 });
        } else if (u::sscanf(line, "vt %f %f", &x0, &y0) == 2
                || u::sscanf(line, "vt %f %f %f", &x0, &y0, &z0) == 3) {
            m_texCoords.push_back({ x0, y0 });
        } else if (u::sscanf(line, "ent %i %f %f %f %f %f %f %f",
                        &i, &x0, &y0, &z0, &x1, &y1, &z1, &w) == 8)
        {
            kdEnt ent;
            ent.id = i;
            ent.origin = { x0, y0, z0 };
            ent.rotation = { x1, y1, z1, w };
            m_entities.push_back(ent);
        } else if (u::sscanf(line, "f %i/%i %i/%i %i/%i",
                        &v0, &t0, &v1, &t1, &v2, &t2) == 6
               ||  u::sscanf(line, "f %i/%i/%i %i/%i/%i %i/%i/%i",
                        &v0, &t0, &s0, &v1, &t1, &s1, &v2, &t2, &s2) == 9)
        {
            kdTriangle triangle;
            triangle.m_textureReference = textureReference;
            triangle.m_vertices[0] = v0 - 1;
            triangle.m_vertices[1] = v1 - 1;
            triangle.m_vertices[2] = v2 - 1;
            triangle.m_texCoords[0] = t0 - 1;
            triangle.m_texCoords[1] = t1 - 1;
            triangle.m_texCoords[2] = t2 - 1;
            triangle.generatePlane(this);
            m_triangles.push_back(triangle);
        } else if (u::sscanf(line, "f %i %i %i", &v0, &v1, &v2) == 3) {
            kdTriangle triangle;
            triangle.m_textureReference = textureReference;
            triangle.m_vertices[0] = v0 - 1;
            triangle.m_vertices[1] = v1 - 1;
            triangle.m_vertices[2] = v2 - 1;
            triangle.generatePlane(this);
            m_triangles.push_back(triangle);
        } else if (u::sscanf(line, "g slot%d", &slot) == 1) {
            m_textureCount++;
            auto format = u::format("textures/%d", slot);
            auto find = m_textures.find(format);
            if (find == m_textures.end())
                find = m_textures.insert(format).first;
            textureReference = &*find;
        }
    }

    m_nodeCount = 0;
    m_leafCount = 0;
    u::vector<int> indices;
    indices.reserve(m_triangles.size());
    for (size_t i = 0; i < m_triangles.size(); i++)
        indices.push_back(i);

    m_root = new kdNode(this, indices, 0);
    return true;
}

static uint32_t kdBinAddTexture(u::vector<kdBinTexture> &textures, const u::string &texturePath) {
    uint32_t index = 0;
    for (const auto &it : textures) {
        if (it.name == texturePath)
            return index;
        index++;
    }
    kdBinTexture texture;
    // Truncate the string if it doesn't fit
    const size_t length = u::min(sizeof texture.name - 1, texturePath.size());
    memcpy(texture.name, (const void *)texturePath.c_str(), length);
    texture.name[length] = '\0';
    textures.push_back(texture);
    return textures.size() - 1;
}

static void kdBinCalculateTangent(const u::vector<kdBinTriangle> &triangles, const u::vector<kdBinVertex> &vertices,
    size_t index, m::vec3 &faceNormal, m::vec3 &tangent, m::vec3 &bitangent)
{
    const m::vec3 &x = vertices[triangles[index].v[0]].vertex;
    const m::vec3 &y = vertices[triangles[index].v[1]].vertex;
    const m::vec3 &z = vertices[triangles[index].v[2]].vertex;
    const m::vec3 q1(y - x);
    const m::vec3 q2(z - x);
    const m::vec2 &c0 = vertices[triangles[index].v[0]].coordinate;
    const m::vec2 &c1 = vertices[triangles[index].v[1]].coordinate;
    const m::vec2 &c2 = vertices[triangles[index].v[2]].coordinate;
    const float s1 = c1.x - c0.x;
    const float s2 = c2.x - c0.x;
    const float t1 = c1.y - c0.y;
    const float t2 = c2.y - c0.y;
    const float det = s1*t2 - s2*t1;
    if (m::abs(det) <= m::kEpsilon) {
        // Unable to compute tangent + bitangent, default tangent along xAxis and
        // bitangent along yAxis.
        tangent = m::vec3::xAxis;
        bitangent = m::vec3::yAxis;
        faceNormal = (y - x) ^ (z - x);
        return;
    }

    const float inv = 1.0f / det;
    tangent = m::vec3(inv * (t2 * q1.x - t1 * q2.x),
                      inv * (t2 * q1.y - t1 * q2.y),
                      inv * (t2 * q1.z - t1 * q2.z));
    bitangent = m::vec3(inv * (-s2 * q1.x + s1 * q2.x),
                        inv * (-s2 * q1.y + s1 * q2.y),
                        inv * (-s2 * q1.z + s1 * q2.z));
    faceNormal = (y - x) ^ (z - x);
}

static void kdBinCreateTangents(u::vector<kdBinVertex> &vertices, const u::vector<kdBinTriangle> &triangles) {
    // Computing Tangent Space Basis Vectors for an Arbitrary Mesh (Lengyel’s Method)
    // Section 7.8 (or in Section 6.8 of the second edition).
    const size_t vertexCount = vertices.size();
    const size_t triangleCount = triangles.size();
    u::vector<m::vec3> normals(vertexCount);
    u::vector<m::vec3> tangents(vertexCount);
    u::vector<m::vec3> bitangents(vertexCount);
    m::vec3 normal;
    m::vec3 tangent;
    m::vec3 bitangent;

    for (size_t i = 0; i < triangleCount; i++) {
        kdBinCalculateTangent(triangles, vertices, i, normal, tangent, bitangent);
        const size_t x = triangles[i].v[0];
        const size_t y = triangles[i].v[1];
        const size_t z = triangles[i].v[2];

        normals[x] += normal;
        normals[y] += normal;
        normals[z] += normal;
        tangents[x] += tangent;
        tangents[y] += tangent;
        tangents[z] += tangent;
        bitangents[x] += bitangent;
        bitangents[y] += bitangent;
        bitangents[z] += bitangent;
    }

    for (size_t i = 0; i < vertexCount; i++) {
        // Gram-Schmidt orthogonalize
        // http://en.wikipedia.org/wiki/Gram%E2%80%93Schmidt_process
        vertices[i].normal = normals[i].normalized();
        const m::vec3 &n = vertices[i].normal;
        m::vec3 t = tangents[i];
        m::vec3 tangent = (t - n * (n * t)).normalized();

        if (!tangent.isNormalized()) {
            // Couldn't calculate vertex tangent for vertex, so we fill it in along
            // the x axis.
            tangent = m::vec3::xAxis;
            t = tangent;
        }

        // bitangents are only stored by handness in the W component (-1.0f or 1.0f).
        vertices[i].tangent = m::vec4(tangent, (((n ^ t) * bitangents[i]) < 0.0f) ? -1.0f : 1.0f);
    }
}

static bool kdBinCompare(const kdBinVertex &lhs, const kdBinVertex &rhs, float epsilon) {
    return lhs.vertex.equals(rhs.vertex, epsilon)
        && (m::abs(lhs.coordinate.x - rhs.coordinate.x) < epsilon)
        && (m::abs(lhs.coordinate.y - rhs.coordinate.y) < epsilon);
}

static int32_t kdBinInsertLeaf(const kdNode *leaf, u::vector<kdBinLeaf> &leafs) {
    kdBinLeaf binLeaf;
    binLeaf.triangles.insert(binLeaf.triangles.begin(), leaf->m_triangles.begin(), leaf->m_triangles.end());
    leafs.push_back(binLeaf);
    return -(int32_t)leafs.size(); // leaf indices are stored with negative index
}

static int32_t kdBinGetNodes(const kdTree &tree, const kdNode *node, u::vector<kdBinPlane> &planes,
    u::vector<kdBinNode> &nodes, u::vector<kdBinLeaf> &leafs)
{
    if (node->isLeaf())
        return kdBinInsertLeaf(node, leafs);

    // We only care about the distance and axis type for the plane.
    kdBinPlane binPlane;
    binPlane.d = node->m_splitPlane.d;
    for (size_t i = 0; i < 3; i++) {
        if (m::abs(node->m_splitPlane.n[i]) > m::kEpsilon) {
            binPlane.type = i;
            break;
        }
    }
    planes.push_back(binPlane);
    const size_t planeIndex = planes.size() - 1;
    const size_t nodeIndex = nodes.size();
    kdBinNode binNode;
    binNode.plane = planeIndex;
    binNode.sphereRadius = node->m_sphereRadius;
    binNode.sphereOrigin = node->m_sphereOrigin;
    nodes.push_back(binNode);

    nodes[nodeIndex].children[0] = kdBinGetNodes(tree, node->m_front, planes, nodes, leafs);
    nodes[nodeIndex].children[1] = kdBinGetNodes(tree, node->m_back, planes, nodes, leafs);

    return nodeIndex;
}
template <typename T>
static void kdSerialize(u::vector<unsigned char> &buffer, const T *data, size_t size) {
    const unsigned char *const beg = (const unsigned char *const)data;
    const unsigned char *const end = ((const unsigned char *const)data) + size;
    buffer.insert(buffer.end(), beg, end);
}

template <typename T>
static void kdSerializeEntry(u::vector<unsigned char> &buffer, T &data) {
    data.endianSwap();
    kdSerialize(buffer, &data, sizeof data);
}

template <typename T>
inline void kdSerializeLump(u::vector<unsigned char> &buffer, const u::vector<T> &lump) {
    for (auto &it : lump)
        kdSerialize(buffer, &it, sizeof it);
}

template <>
inline void kdSerializeLump<kdBinLeaf>(u::vector<unsigned char> &buffer, const u::vector<kdBinLeaf> &leafs) {
    for (const auto &it : leafs) {
        const uint32_t triangleCount = it.triangles.size();
        const uint32_t serializeCount = u::endianSwap(triangleCount);
        kdSerialize(buffer, &serializeCount, sizeof serializeCount);
        for (size_t i = 0; i < triangleCount; i++) {
            const uint32_t triangleIndex = u::endianSwap(it.triangles[i]);
            kdSerialize(buffer, &triangleIndex, sizeof triangleIndex);
        }
    }
}

u::vector<unsigned char> kdTree::serialize() {
    u::vector<kdBinPlane>    compiledPlanes;
    u::vector<kdBinTexture>  compiledTextures;
    u::vector<kdBinNode>     compiledNodes;
    u::vector<kdBinTriangle> compiledTriangles;
    u::vector<kdBinVertex>   compiledVertices;
    u::vector<kdBinEnt>      compiledEntities;
    u::vector<kdBinLeaf>     compiledLeafs;

    // reserve memory for this operation
    compiledPlanes.reserve(m_nodeCount - m_leafCount);
    compiledTextures.reserve(m_textureCount);
    compiledNodes.reserve(m_nodeCount - m_leafCount);
    compiledTriangles.reserve(m_triangles.size());
    compiledVertices.reserve(m_triangles.size() * 3);
    compiledLeafs.reserve(m_leafCount);

    for (size_t i = 0; i < m_triangles.size(); i++) {
        kdBinTriangle triangle;
        triangle.texture = kdBinAddTexture(compiledTextures, *m_triangles[i].m_textureReference);
        for (size_t j = 0; j < 3; j++) {
            kdBinVertex vertex;
            vertex.vertex = m_vertices[m_triangles[i].m_vertices[j]];
            if (!m_texCoords.empty())
                vertex.coordinate = m_texCoords[m_triangles[i].m_texCoords[j]];
            // If we can reuse vertices for several faces, then do so
            int k = 0;
            for (k = compiledVertices.size() - 1; k >= 0; k--) {
                if (kdBinCompare(compiledVertices[k], vertex, kEpsilon))
                    break;
            }
            if ((size_t)k >= compiledVertices.size()) {
                // no mathing vertex found
                compiledVertices.push_back(vertex);
                k = compiledVertices.size() - 1;
            }
            triangle.v[j] = k;
        }
        compiledTriangles.push_back(triangle);
    }

    kdBinGetNodes(*this, m_root, compiledPlanes, compiledNodes, compiledLeafs);

    // Get entities
    for (const auto &it : m_entities) {
        kdBinEnt ent;
        ent.id = it.id;
        ent.origin = it.origin;
        ent.rotation = it.rotation;
        compiledEntities.push_back(ent);
    }

    kdBinCreateTangents(compiledVertices, compiledTriangles);

    if (compiledNodes.size() == 0) {
        kdBinNode emptyNode;
        emptyNode.children[0] = -1;
        emptyNode.children[1] = -1;
        emptyNode.sphereRadius = kMaxTraceDistance - 1.0f;
        emptyNode.sphereOrigin = m::vec3::origin;
        emptyNode.plane = 0;

        kdBinPlane emptyPlane;
        emptyPlane.type = 0; // X
        emptyPlane.d = 0;

        compiledNodes.push_back(emptyNode);
        compiledPlanes.push_back(emptyPlane);
    }

    if (compiledEntities.size() == 0) {
        kdBinEnt emptyEnt;
        compiledEntities.push_back(emptyEnt);
    }

    kdBinEntry entryPlanes;
    kdBinEntry entryTextures;
    kdBinEntry entryNodes;
    kdBinEntry entryTriangles;
    kdBinEntry entryVertices;
    kdBinEntry entryEntities;
    kdBinEntry entryLeafs;
    kdBinHeader header;

    entryPlanes.offset = sizeof header + 7*sizeof(kdBinEntry);
    entryPlanes.length = compiledPlanes.size() * sizeof(kdBinPlane);
    entryTextures.offset = entryPlanes.length + entryPlanes.offset;
    entryTextures.length = compiledTextures.size() * sizeof(kdBinTexture);
    entryNodes.offset = entryTextures.length + entryTextures.offset;
    entryNodes.length = compiledNodes.size() * sizeof(kdBinNode);
    entryTriangles.offset = entryNodes.length + entryNodes.offset;
    entryTriangles.length = compiledTriangles.size() * sizeof(kdBinTriangle);
    entryVertices.offset = entryTriangles.length + entryTriangles.offset;
    entryVertices.length = compiledVertices.size() * sizeof(kdBinVertex);
    entryEntities.offset = entryVertices.length + entryVertices.offset;
    entryEntities.length = compiledEntities.size() * sizeof(kdBinEnt);
    entryLeafs.offset = entryEntities.length + entryEntities.offset;
    entryLeafs.length = compiledLeafs.size();

    u::vector<unsigned char> store;

    kdSerializeEntry(store, header);
    kdSerializeEntry(store, entryPlanes);
    kdSerializeEntry(store, entryTextures);
    kdSerializeEntry(store, entryNodes);
    kdSerializeEntry(store, entryTriangles);
    kdSerializeEntry(store, entryVertices);
    kdSerializeEntry(store, entryEntities);
    kdSerializeEntry(store, entryLeafs);
    kdSerializeLump(store, compiledPlanes);
    kdSerializeLump(store, compiledTextures);
    kdSerializeLump(store, compiledNodes);
    kdSerializeLump(store, compiledTriangles);
    kdSerializeLump(store, compiledVertices);
    kdSerializeLump(store, compiledEntities);
    kdSerializeLump(store, compiledLeafs);

    const uint32_t end = u::endianSwap(kdBinHeader::kMagic);
    kdSerialize(store, &end, sizeof end);

    // compress
    u::vector<unsigned char> compressed;
    compressed.reserve(store.size() * 2);
    u::zlib::compress(compressed, store);

    return compressed;
}
