#include "model.h"
#include "engine.h"

#include "u_file.h"
#include "u_map.h"

#include "m_quat.h"
#include "m_half.h"

/// Tangent and Bitangent calculation
static void calculateTangent(const u::vector<m::vec3> &vertices,
                             const u::vector<m::vec2> &coordinates,
                             size_t v0,
                             size_t v1,
                             size_t v2,
                             m::vec3 &tangent,
                             m::vec3 &bitangent)
{
    const m::vec3 &x = vertices[v0];
    const m::vec3 &y = vertices[v1];
    const m::vec3 &z = vertices[v2];
    const m::vec3 q1(y - x);
    const m::vec3 q2(z - x);
    const float s1 = coordinates[v1].x - coordinates[v0].x;
    const float s2 = coordinates[v2].x - coordinates[v0].x;
    const float t1 = coordinates[v1].y - coordinates[v0].y;
    const float t2 = coordinates[v2].y - coordinates[v0].y;
    const float det = s1*t2 - s2*t1;
    if (m::abs(det) <= m::kEpsilon) {
        // Unable to compute tangent + bitangent, default tangent along xAxis and
        // bitangent along yAxis.
        tangent = m::vec3::xAxis;
        bitangent = m::vec3::yAxis;
        return;
    }

    const float inv = 1.0f / det;
    tangent = m::vec3(inv * (t2 * q1.x - t1 * q2.x),
                      inv * (t2 * q1.y - t1 * q2.y),
                      inv * (t2 * q1.z - t1 * q2.z));
    bitangent = m::vec3(inv * (-s2 * q1.x + s1 * q2.x),
                        inv * (-s2 * q1.y + s1 * q2.y),
                        inv * (-s2 * q1.z + s1 * q2.z));
}

static void createTangents(const u::vector<m::vec3> &vertices,
                           const u::vector<m::vec2> &coordinates,
                           const u::vector<m::vec3> &normals,
                           const u::vector<size_t> &indices,
                           u::vector<m::vec3> &tangents_,
                           u::vector<float> &bitangents_)
{
    // Computing Tangent Space Basis Vectors for an Arbitrary Mesh (Lengyel’s Method)
    // Section 7.8 (or in Section 6.8 of the second edition).
    const size_t vertexCount = vertices.size();
    u::vector<m::vec3> tangents(vertexCount);
    u::vector<m::vec3> bitangents(vertexCount);
    m::vec3 tangent;
    m::vec3 bitangent;

    for (size_t i = 0; i < indices.size(); i += 3) {
        const size_t x = indices[i+0];
        const size_t y = indices[i+1];
        const size_t z = indices[i+2];

        calculateTangent(vertices, coordinates, x, y, z, tangent, bitangent);

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
        const m::vec3 &n = normals[i];
        m::vec3 t = tangents[i];
        const m::vec3 v = (t - n * (n * t));
        tangents_[i] = v.isNullEpsilon() ? v : v.normalized();

        if (!tangents_[i].isNormalized()) {
            // Couldn't calculate vertex tangent for vertex, so we fill it in along
            // the x axis.
            tangents_[i] = m::vec3::xAxis;
            t = tangents_[i];
        }

        // bitangents are only stored by handness in the W component (-1.0f or 1.0f).
        bitangents_[i] = (((n ^ t) * bitangents[i]) < 0.0f) ? -1.0f : 1.0f;
    }
}

///! OBJ Loader
struct obj {
    bool load(const u::string &file, model *store);
};

bool obj::load(const u::string &file, model *store) {
    u::file fp = fopen(neoGamePath() + file + ".obj", "r");
    if (!fp)
        return false;

    // Processed vertices, normals and coordinates from the OBJ file
    u::vector<m::vec3> vertices;
    u::vector<m::vec3> normals;
    u::vector<m::vec2> coordinates;
    u::vector<m::vec3> tangents;
    u::vector<float> bitangents;

    // Unique vertices are stored in a map keyed by face.
    u::map<face, size_t> uniques;

    // Current group and indices for group
    u::string group;
    u::map<u::string, u::vector<size_t>> groups;

    size_t count = 0;
    for (u::string line; u::getline(fp, line); ) {
        // Skip whitespace
        while (line.size() && strchr(" \t", line[0]))
            line.pop_front();
        // Skip comments
        if (strchr("#$", line[0]))
            continue;
        // Skip empty lines
        if (line.empty())
            continue;

        // Process the individual lines
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        if (u::sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
            // v float float float
            vertices.push_back({x, y, z * -1.0f});
        } else if (u::sscanf(line, "vn %f %f %f", &x, &y, &z) == 3) {
            // vn float float float
            normals.push_back({x * -1.0f, y * -1.0f, z});
        } else if (u::sscanf(line, "vt %f %f", &x, &y) == 2) {
            // vt float float
            coordinates.push_back({x, 1.0f - y});
        } else if (line[0] == 'g') {
            // Read a group name
            const char *what;
            for (what = &line[0] + 1; *what && strchr(" \t", *what); what++)
                ;
            group = what;
        } else if (line[0] == 'f') {
            u::vector<size_t> v;
            u::vector<size_t> n;
            u::vector<size_t> t;

            // Note: 1 to skip "f"
            auto contents = u::split(line);
            for (size_t i = 1; i < contents.size(); i++) {
                int vi = 0;
                int ni = 0;
                int ti = 0;
                if (u::sscanf(contents[i], "%i/%i/%i", &vi, &ti, &ni) == 3) {
                    v.push_back(vi < 0 ? v.size() + vi : vi - 1);
                    t.push_back(ti < 0 ? t.size() + ti : ti - 1);
                    n.push_back(ni < 0 ? n.size() + ni : ni - 1);
                } else if (u::sscanf(contents[i], "%i//%i", &vi, &ni) == 2) {
                    v.push_back(vi < 0 ? v.size() + vi : vi - 1);
                    n.push_back(ni < 0 ? n.size() + ni : ni - 1);
                } else if (u::sscanf(contents[i], "%i/%i", &vi, &ti) == 2) {
                    v.push_back(vi < 0 ? v.size() + vi : vi - 1);
                    t.push_back(ti < 0 ? t.size() + ti : ti - 1);
                } else if (u::sscanf(contents[i], "%i", &vi) == 1) {
                    v.push_back(vi < 0 ? v.size() + vi : vi - 1);
                }
            }

            // Triangulate the mesh
            for (size_t i = 1; i < v.size() - 1; ++i) {
                auto &indices = groups[group];
                const size_t index = indices.size();
                indices.resize(index + 3);
                auto triangulate = [&v, &n, &t, &uniques, &count](size_t index, size_t &out) {
                    face triangle;
                    triangle.vertex = v[index];
                    if (n.size()) triangle.normal = n[index];
                    if (t.size()) triangle.coordinate = t[index];
                    // Only insert in the map if it doesn't exist
                    if (uniques.find(triangle) == uniques.end())
                        uniques[triangle] = count++;
                    out = uniques[triangle];
                };
                triangulate(0,     indices[index + 2]);
                triangulate(i + 0, indices[index + 1]);
                triangulate(i + 1, indices[index + 0]);
            }
        }
    }

    // Construct the model, indices are already generated
    u::vector<m::vec3> positions_(count);
    u::vector<m::vec3> normals_(count);
    u::vector<m::vec2> coordinates_(count);
    for (const auto &it : uniques) {
        const auto &first = it.first;
        const auto &second = it.second;
        positions_[second] = vertices[first.vertex];
        if (normals.size())
            normals_[second] = normals[first.normal];
        if (coordinates.size())
            coordinates_[second] = coordinates[first.coordinate];
    }

    // Optimize indices
    u::vector<size_t> indices_;
    for (auto &g : groups) {
        // Optimize the indices
        vertexCacheOptimizer vco;
        vco.optimize(g.second);
        indices_.reserve(indices_.size() + g.second.size());
        for (const auto &i : g.second)
            indices_.push_back(i);
    }

    // Calculate tangents
    u::vector<m::vec3> tangents_(count);
    u::vector<float> bitangents_(count);
    createTangents(positions_, coordinates_, normals_, indices_, tangents_, bitangents_);

    // Interleave vertex data for GPU
    store->m_basicVertices.resize(count);
    for (size_t i = 0; i < count; i++) {
        auto &vert = store->m_basicVertices[i];
        for (size_t j = 0; j < 3; j++) {
            vert.position[j] = positions_[i][j];
            vert.normal[j] = normals_[i][j];
            vert.tangent[j] = tangents_[i][j];
        }
        vert.coordinate[0] = coordinates_[i].x;
        vert.coordinate[1] = coordinates_[i].y;
        vert.tangent[3] = bitangents_[i];
    }

    // Generate batches
    for (const auto &g : groups) {
        model::batch b;
        b.offset = (void *)(store->m_indices.size() * sizeof(uint32_t));
        b.count = g.second.size();
        // Rewire size_t -> GLuint
        store->m_indices.reserve(store->m_indices.size() + b.count);
        for (const auto &i : g.second)
            store->m_indices.push_back(i);
        // Emit mesh names in same order as materials
        store->m_meshNames.push_back(g.first);
        store->m_batches.push_back(b);
    }

    return true;
}

///! IQM loader
struct iqm {
    static constexpr int kUByte = 1;
    static constexpr int kUInt = 5;
    static constexpr int kHalf = 6;
    static constexpr int kFloat = 7;

    bool load(const u::string &file, model *store, const u::vector<u::string> &anims);
protected:
    struct iqmHeader {
        static constexpr const char *kMagic = "INTERQUAKEMODEL";
        static constexpr uint32_t kVersion = 2;
        char magic[16];
        uint32_t version;
        uint32_t fileSize;
        uint32_t flags;
        uint32_t numText, ofsText;
        uint32_t numMeshes, ofsMeshes;
        uint32_t numVertexArrays, numVertexes, ofsVertexArrays;
        uint32_t numTriangles, ofsTriangles, ofsAdjacency;
        uint32_t numJoints, ofsJoints;
        uint32_t numPoses, ofsPoses;
        uint32_t numAnims, ofsAnims;
        uint32_t numFrames, numFrameChannels, ofsFrames, ofsBounds;
        uint32_t numComment, ofsComment;
        uint32_t numExtensions, ofsExtensions;
        void endianSwap();
    };

    struct iqmMesh {
        uint32_t name;
        uint32_t material;
        uint32_t firstVertex, numVertexes;
        uint32_t firstTriangle, numTriangles;
    };

    enum {
        kPosition     = 0,
        kTexCoord     = 1,
        kNormal       = 2,
        kTangent      = 3,
        kBlendIndexes = 4,
        kBlendWeights = 5
    };

    struct iqmTriangle {
        uint32_t vertex[3];
    };

    struct iqmJoint {
        uint32_t name;
        int32_t parent;
        float translate[3];
        float rotate[4];
        float scale[3];
    };

    struct iqmPose {
        int32_t parent;
        uint32_t mask;
        float channelOffset[10];
        float channelScale[10];
    };

    struct iqmAnim {
        uint32_t name;
        uint32_t firstFrame, numFrames;
        float frameRate;
        uint32_t flags;
    };

    struct iqmVertexArray {
        uint32_t type;
        uint32_t flags;
        uint32_t format;
        uint32_t size;
        uint32_t offset;
    };

    bool loadMeshes(const iqmHeader *hdr, unsigned char *buf, model *store);
    bool loadAnims(const iqmHeader *hdr, unsigned char *buf, model *store);

private:
    u::vector<m::mat3x4> m_baseFrame;
    u::vector<m::mat3x4> m_inverseBaseFrame;
};

inline void iqm::iqmHeader::endianSwap() {
    u::endianSwap(&version, (sizeof(iqmHeader) - sizeof(magic)) / sizeof(uint32_t));
}

// Helper structure to deal with aliasing input buffer as half precision,
// single precision floats or unsigned int as well as other general utilities
struct inData {
    inData()
        : type(-1)
        , asUByte(nullptr)
    {
    }

    operator bool() const {
        return asUByte;
    }

    void operator=(const u::pair<uint32_t, unsigned char *> &data) {
        type = data.first;
        asUByte = data.second;
    }

    bool isHalf() const { return type == iqm::kHalf; }
    bool isUInt() const { return type == iqm::kUInt; }

    void endianSwap(size_t count) {
        switch (type) {
        case iqm::kHalf:
            u::endianSwap(asHalf, count);
            break;
        case iqm::kFloat:
            u::endianSwap(asFloat, count);
            break;
        case iqm::kUInt:
            u::endianSwap(asUInt, count);
            break;
        }
    }

    int type;

    union {
        unsigned char *asUByte;
        uint32_t *asUInt;
        float *asFloat;
        m::half *asHalf;
    };
};

bool iqm::loadMeshes(const iqmHeader *hdr, unsigned char *buf, model *store) {
    u::endianSwap((uint32_t*)&buf[hdr->ofsVertexArrays],
        hdr->numVertexArrays*sizeof(iqmVertexArray)/sizeof(uint32_t));
    u::endianSwap((uint32_t*)&buf[hdr->ofsTriangles],
        hdr->numTriangles*sizeof(iqmTriangle)/sizeof(uint32_t));
    u::endianSwap((uint32_t*)&buf[hdr->ofsMeshes],
        hdr->numMeshes*sizeof(iqmMesh)/sizeof(uint32_t));
    u::endianSwap((uint32_t*)&buf[hdr->ofsJoints],
        hdr->numJoints*sizeof(iqmJoint)/sizeof(uint32_t));

    inData inPosition;
    inData inNormal;
    inData inTangent;
    inData inCoordinate;
    inData inBlendIndex;
    inData inBlendWeight;

    iqmVertexArray *vertexArrays = (iqmVertexArray*)&buf[hdr->ofsVertexArrays];
    for (uint32_t i = 0; i < hdr->numVertexArrays; i++) {
        iqmVertexArray &va = vertexArrays[i];
        switch (va.type) {
        case iqm::kPosition:
            if ((va.format != kFloat && va.format != kHalf) || va.size != 3)
                return false;
            inPosition = { va.format, buf + va.offset };
            break;
        case iqm::kNormal:
            if ((va.format != kFloat && va.format != kHalf) || va.size != 3)
                return false;
            inNormal = { va.format, buf + va.offset };
            break;
        case iqm::kTangent:
            if ((va.format != kFloat && va.format != kHalf) || va.size != 4)
                return false;
            inTangent = { va.format, buf + va.offset };
            break;
        case iqm::kTexCoord:
            if ((va.format != kFloat && va.format != kHalf) || va.size != 2)
                return false;
            inCoordinate = { va.format, buf + va.offset };
            break;
        case iqm::kBlendIndexes:
            if ((va.format != kUByte && va.format != kUInt) || va.size != 4)
                return false;
            inBlendIndex = { va.format, buf + va.offset };
            break;
        case iqm::kBlendWeights:
            if ((va.format != kUByte && va.format != kUInt) || va.size != 4)
                return false;
            inBlendWeight = { va.format, buf + va.offset };
            break;
        }
    }

    const iqmJoint *const joints = (iqmJoint*)&buf[hdr->ofsJoints];

    const bool animated = hdr->numFrames != 0;
    store->m_numFrames = hdr->numFrames;
    if (animated) {
        store->m_numJoints = hdr->numJoints;
        store->m_outFrame.resize(hdr->numJoints);
        store->m_parents.resize(hdr->numJoints);
        for (uint32_t i = 0; i < hdr->numJoints; i++)
            store->m_parents[i] = joints[i].parent;
        m_baseFrame.resize(hdr->numJoints);
        m_inverseBaseFrame.resize(hdr->numJoints);
        for (uint32_t i = 0; i < hdr->numJoints; i++) {
            const iqmJoint &j = joints[i];
            m_baseFrame[i] = m::mat3x4(m::quat(j.rotate).normalize(),
                                       m::vec3(j.translate),
                                       m::vec3(j.scale));
            m_inverseBaseFrame[i].invert(m_baseFrame[i]);
            if (j.parent >= 0) {
                m_baseFrame[i] = m_baseFrame[j.parent] * m_baseFrame[i];
                m_inverseBaseFrame[i] *= m_inverseBaseFrame[j.parent];
            }
        }
    }

    // indices
    const iqmTriangle *const triangles = (iqmTriangle*)&buf[hdr->ofsTriangles];
    store->m_indices.reserve(hdr->numTriangles);
    for (uint32_t i = 0; i < hdr->numTriangles; i++) {
        const iqmTriangle &triangle = triangles[i];
        store->m_indices.push_back(triangle.vertex[0]);
        store->m_indices.push_back(triangle.vertex[2]);
        store->m_indices.push_back(triangle.vertex[1]);
    }

    if (inPosition)    inPosition.endianSwap(3*hdr->numVertexes);
    if (inNormal)      inNormal.endianSwap(3*hdr->numVertexes);
    if (inTangent)     inTangent.endianSwap(4*hdr->numVertexes);
    if (inCoordinate)  inCoordinate.endianSwap(2*hdr->numVertexes);
    if (inBlendIndex)  inBlendIndex.endianSwap(4*hdr->numVertexes);
    if (inBlendWeight) inBlendWeight.endianSwap(4*hdr->numVertexes);

    // if one attribute is half float, all attributes must be half float
    if (inPosition.isHalf() != inNormal.isHalf() || inNormal.isHalf() != inTangent.isHalf() || inTangent.isHalf() != inCoordinate.isHalf())
        return false;
    bool isHalf = inPosition.isHalf();

    if (animated) {
        store->m_animVertices.resize(hdr->numVertexes);
        for (uint32_t i = 0; i < hdr->numVertexes; i++) {
            unsigned char (*curBlendWeight)[4] = nullptr;
            unsigned char (*curBlendIndex)[4] = nullptr;
            if (isHalf) {
                mesh::animHalfVertex &v = store->m_animHalfVertices[i];
                if (inPosition)    memcpy(v.position, &inPosition.asHalf[i*3], sizeof v.position);
                if (inCoordinate)  memcpy(v.coordinate, &inCoordinate.asHalf[i*2], sizeof v.coordinate);
                if (inTangent)     memcpy(v.tangent, &inTangent.asHalf[i*4], sizeof v.tangent);
                if (inNormal) {
                    for (size_t j = 0; j < 3; j++)
                        inNormal.asHalf[i*3+j] ^= 0x8000;
                    memcpy(v.normal, &inNormal.asHalf[i*3], sizeof v.normal);
                }
                curBlendIndex = &v.blendIndex;
                curBlendWeight = &v.blendWeight;
            } else {
                mesh::animVertex &v = store->m_animVertices[i];
                if (inPosition)    memcpy(v.position, &inPosition.asFloat[i*3], sizeof v.position);
                if (inCoordinate)  memcpy(v.coordinate, &inCoordinate.asFloat[i*2], sizeof v.coordinate);
                if (inTangent)     memcpy(v.tangent, &inTangent.asFloat[i*4], sizeof v.tangent);
                if (inNormal) {
                    // Flip normals
                    const m::vec3 flip = -1.0f * m::vec3(inNormal.asFloat[i*3+0],
                                                         inNormal.asFloat[i*3+1],
                                                         inNormal.asFloat[i*3+2]);
                    memcpy(v.normal, &flip.x, sizeof v.normal);
                }
                curBlendIndex = &v.blendIndex;
                curBlendWeight = &v.blendWeight;
            }
            if (inBlendIndex) {
                if (inBlendIndex.isUInt()) {
                    uint32_t indices[4];
                    memcpy(indices, &inBlendIndex.asUInt[i*4], sizeof indices);
                    for (size_t i = 0; i < 4; i++)
                        (*curBlendIndex)[i] = indices[i] & 0xFF;
                } else {
                    memcpy(*curBlendIndex, &inBlendIndex.asUByte[i*4], 4);
                }
            }
            if (inBlendWeight) {
                if (inBlendWeight.isUInt()) {
                    uint32_t weights[4];
                    memcpy(weights, &inBlendWeight.asUInt[i*4], sizeof weights);
                    for (size_t i = 0; i < 4; i++)
                        (*curBlendWeight)[i] = weights[i] & 0xFF;
                } else {
                    memcpy(*curBlendWeight, &inBlendWeight.asUByte[i*4], 4);
                }
            }
        }
    } else {
        store->m_basicVertices.resize(hdr->numVertexes);
        for (uint32_t i = 0; i < hdr->numVertexes; i++) {
            if (isHalf) {
                mesh::basicHalfVertex &v = store->m_basicHalfVertices[i];
                if (inPosition)   memcpy(v.position, &inPosition.asHalf[i*3], sizeof v.position);
                if (inCoordinate) memcpy(v.coordinate, &inCoordinate.asHalf[i*2], sizeof v.coordinate);
                if (inTangent)    memcpy(v.tangent, &inTangent.asHalf[i*4], sizeof v.tangent);
                if (inNormal) {
                    for (size_t j = 0; j < 3; j++)
                        inNormal.asHalf[i*3+j] ^= 0x8000;
                    memcpy(v.normal, &inNormal.asHalf[i*3], sizeof v.normal);
                }
            } else {
                mesh::basicVertex &v = store->m_basicVertices[i];
                if (inPosition)   memcpy(v.position, &inPosition.asFloat[i*3], sizeof v.position);
                if (inCoordinate) memcpy(v.coordinate, &inCoordinate.asFloat[i*2], sizeof v.coordinate);
                if (inTangent)    memcpy(v.tangent, &inTangent.asFloat[i*4], sizeof v.tangent);
                if (inNormal) {
                    // Flip normals
                    const m::vec3 flip = -1.0f * m::vec3(inNormal.asFloat[i*3+0],
                                                         inNormal.asFloat[i*3+1],
                                                         inNormal.asFloat[i*3+2]);
                    memcpy(v.normal, &flip.x, sizeof v.normal);
                }
            }
        }
    }
    store->m_isHalf = isHalf;
    return true;
}

bool iqm::loadAnims(const iqmHeader *hdr, unsigned char *buf, model *store) {
    u::endianSwap((uint32_t *)&buf[hdr->ofsPoses], hdr->numPoses*sizeof(iqmPose)/sizeof(uint32_t));
    u::endianSwap((uint32_t *)&buf[hdr->ofsAnims], hdr->numAnims*sizeof(iqmAnim)/sizeof(uint32_t));
    u::endianSwap((uint16_t *)&buf[hdr->ofsFrames], hdr->numFrames*hdr->numFrameChannels);

    const iqmPose *const poses = (iqmPose*)&buf[hdr->ofsPoses];

    const size_t size = store->m_frames.size();
    store->m_frames.resize(size + hdr->numFrames * hdr->numPoses);
    const uint16_t *frameData = (uint16_t*)&buf[hdr->ofsFrames];
    for (uint32_t i = 0; i < hdr->numFrames; i++) {
        for (uint32_t j = 0; j < hdr->numPoses; j++) {
            const iqmPose &p = poses[j];
            float data[10];
            memcpy(data, p.channelOffset, sizeof p.channelOffset);
            for (size_t v = 0; v < 10; v++)
                if (p.mask & (1 << v))
                    data[v] += (*frameData++) * p.channelScale[v];
            const m::vec3 translate((const float (&)[3])data[0]);
            const m::quat rotate((const float (&)[4])data[3]);
            const m::vec3 scale((const float (&)[3])data[7]);
            const m::mat3x4 m(rotate.normalize(), translate, scale);
            store->m_frames[size + (i*hdr->numPoses + j)] =
                p.parent >= 0
                    ? m_baseFrame[p.parent] * m * m_inverseBaseFrame[j]
                    : m * m_inverseBaseFrame[j];
        }
    }

    return true;
}

bool iqm::load(const u::string &file, model *store, const u::vector<u::string> &anims) {
    auto read = u::read(neoGamePath() + file + ".iqm", "rb");
    if (!read)
        return false;
    auto &data = *read;

    iqmHeader *const hdr = (iqmHeader*)&data[0];
    if (memcmp(hdr->magic, (const void *)iqmHeader::kMagic, sizeof hdr->magic))
        return false;

    hdr->endianSwap();
    if (hdr->version != iqmHeader::kVersion)
        return false;
    if (hdr->numMeshes > 0 && !loadMeshes(hdr, &data[0], store))
        return false;
    if (hdr->numAnims > 0 && !loadAnims(hdr, &data[0], store))
        return false;

    // batches
    model::batch b;
    const char *const str = hdr->ofsText ? (char *)&data[hdr->ofsText] : nullptr;
    const iqmMesh *const meshes = (iqmMesh*)&data[hdr->ofsMeshes];
    for (uint32_t i = 0; i < hdr->numMeshes; i++) {
        const iqmMesh &m = meshes[i];
        iqmTriangle *tri = nullptr;
        b.offset = &tri[m.firstTriangle];
        b.count = 3*m.numTriangles;
        store->m_meshNames.push_back(str ? &str[m.name] : "default");
        store->m_batches.push_back(b);
    }

    // load optional animation files
    for (const auto &it : anims) {
        const auto fileName = u::format("%s/%s.iqm", neoGamePath(), it);
        const auto readAnim = u::read(fileName, "rb");
        // this silently ignores animation files which are not valid or correct
        // version IQM files or cannot be opened (permission, non existent, etc.)
        if (!readAnim)
            continue;
        auto &animData = *read;
        iqmHeader *const animHdr = (iqmHeader*)&animData[0];
        if (memcmp(animHdr->magic, (const void *)iqmHeader::kMagic, sizeof animHdr->magic))
            continue;
        animHdr->endianSwap();
        if (animHdr->version != iqmHeader::kVersion)
            continue;
        if (animHdr->numAnims > 0 && !loadAnims(animHdr, &animData[0], store))
            continue;
        u::print("[model] => loaded animation `%s' for `%s'\n", it, u::fixPath(file));
    }

    return true;
}

model::~model() = default;

void model::makeHalf() {
    static constexpr size_t kFloats = sizeof(mesh::basicVertex)/sizeof(float);
    if (animated()) {
        const auto &vertices = m_animVertices;
        u::vector<mesh::basicVertex> basics(vertices.size());
        for (size_t i = 0; i < vertices.size(); i++)
            memcpy(&basics[i], &vertices[i], sizeof basics[0]);
        const auto halfData = m::convertToHalf((const float *const)&basics[0], kFloats*vertices.size());
        u::vector<mesh::animHalfVertex> converted(vertices.size());
        for (size_t i = 0; i < vertices.size(); i++) {
            memcpy(&converted[i], &halfData[kFloats*i], kFloats*sizeof(m::half));
            memcpy(converted[i].blendWeight, vertices[i].blendWeight, sizeof vertices[0].blendWeight);
            memcpy(converted[i].blendIndex, vertices[i].blendIndex, sizeof vertices[0].blendIndex);
        }
        m_animHalfVertices = u::move(converted);
        m_animVertices.destroy();
    } else {
        const auto &vertices = m_basicVertices;
        u::vector<mesh::basicVertex> basics(vertices.size());
        memcpy(&basics[0], &vertices[0], sizeof basics[0] * vertices.size());
        const auto convert = m::convertToHalf((const float *const)&basics[0], kFloats*vertices.size());
        m_basicHalfVertices.resize(vertices.size());
        memcpy(&m_basicHalfVertices[0], &convert[0], sizeof m_basicHalfVertices[0] * vertices.size());
        m_basicVertices.destroy();
    }
}

void model::makeSingle() {
    static constexpr size_t kHalfs = sizeof(mesh::basicHalfVertex)/sizeof(m::half);
    if (animated()) {
        const auto &vertices = m_animHalfVertices;
        u::vector<mesh::basicHalfVertex> basics(vertices.size());
        for (size_t i = 0; i < vertices.size(); i++)
            memcpy(&basics[i], &vertices[i], sizeof basics[0]);
        const auto singleData = m::convertToFloat((const m::half *const)&basics[0], kHalfs*vertices.size());
        u::vector<mesh::animVertex> converted(vertices.size());
        for (size_t i = 0; i < vertices.size(); i++) {
            memcpy(&converted[i], &singleData[kHalfs*i], kHalfs*sizeof(float));
            memcpy(converted[i].blendWeight, vertices[i].blendWeight, sizeof vertices[0].blendWeight);
            memcpy(converted[i].blendIndex, vertices[i].blendIndex, sizeof vertices[0].blendIndex);
        }
        m_animVertices = u::move(converted);
        m_animHalfVertices.destroy();
    } else {
        const auto &vertices = m_basicHalfVertices;
        static constexpr size_t kHalfs = sizeof(mesh::basicHalfVertex)/sizeof(m::half);
        const auto convert = m::convertToFloat((const m::half *const)&vertices[0], kHalfs*vertices.size());
        m_basicVertices.resize(vertices.size());
        memcpy(&m_basicVertices[0], &convert[0], sizeof m_basicVertices[0] * vertices.size());
        m_basicHalfVertices.destroy();
    }
}

bool model::load(const u::string &file, const u::vector<u::string> &anims) {
    const auto iqm_ = u::format("%s/%s.iqm", neoGamePath(), file);
    const auto obj_ = u::format("%s/%s.obj", neoGamePath(), file);
    if (u::exists(iqm_) && !iqm().load(file, this, anims))
        return false;
    else if (u::exists(obj_) && !obj().load(file, this))
        return false;
    // calculate bounds
    if (animated()) {
        for (const auto &it : m_animVertices)
            m_bounds.expand(m::vec3(it.position));
    } else {
        for (const auto &it : m_basicVertices)
            m_bounds.expand(m::vec3(it.position));
    }
    m_name = file;
    return true;
}

void model::animate(float curFrame) {
    if (m_numFrames == 0)
        return;

    int frame1 = int(m::floor(curFrame));
    int frame2 = frame1 + 1;

    const float frameOffset = curFrame - frame1;

    frame1 %= m_numFrames;
    frame2 %= m_numFrames;

    const m::mat3x4 *const mat1 = &m_frames[frame1 * m_numJoints];
    const m::mat3x4 *const mat2 = &m_frames[frame2 * m_numJoints];

    // Interpolate matrices between the two closest frames and concatenate with
    // parent matrix if necessary.
    // Concatenate the result with the inverse base pose.
    for (size_t i = 0; i < m_numJoints; i++) {
        const m::mat3x4 mat = mat1[i]*(1 - frameOffset) + mat2[i] * frameOffset;
        if (m_parents[i] >= 0)
            m_outFrame[i] = m_outFrame[m_parents[i]] * mat;
        else
            m_outFrame[i] = mat;
    }

}
