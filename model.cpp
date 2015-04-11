#include "model.h"
#include "engine.h"

#include "u_file.h"
#include "u_map.h"

/// Tangent and Bitangent calculation
static void calculateTangent(const u::vector<m::vec3> &vertices,
                             const u::vector<m::vec3> &coordinates,
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
                           const u::vector<m::vec3> &coordinates,
                           const u::vector<m::vec3> &normals,
                           const u::vector<size_t> &indices,
                           u::vector<m::vec3> &tangents_,
                           u::vector<float> &bitangents_)
{
    // Computing Tangent Space Basis Vectors for an Arbitrary Mesh (Lengyel’s Method)
    // Section 7.8 (or in Section 6.8 of the second edition).
    const size_t vertexCount = vertices.size();
    u::vector<m::vec3> tangents;
    u::vector<m::vec3> bitangents;

    tangents.resize(vertexCount);
    bitangents.resize(vertexCount);

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
    bool load(const u::string &file, u::vector<mesh*> &meshes);
};

bool obj::load(const u::string &file, u::vector<mesh*> &meshes) {
    u::file fp = fopen(neoGamePath() + file + ".obj", "r");
    if (!fp)
        return false;

    // Processed vertices, normals and coordinates from the OBJ file
    u::vector<m::vec3> vertices;
    u::vector<m::vec3> normals;
    u::vector<m::vec3> coordinates;
    u::vector<m::vec3> tangents;
    u::vector<float> bitangents;

    // Unique vertices are stored in a map keyed by face.
    u::map<face, size_t> uniques;

    // The indices (which get rewired to unsigned ints)
    u::vector<size_t> indices;

    size_t count = 0;
    size_t group = 0;
    while (auto get = u::getline(fp)) {
        auto &line = *get;
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
            coordinates.push_back({x, 1.0f - y, 0.0f});
        } else if (line[0] == 'g') {
            group++;
        } else if (line[0] == 'f' && group == 0) { // Only process the first group faces
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
                auto index = indices.size();
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
                triangulate(0,     indices[index + 0]);
                triangulate(i + 0, indices[index + 1]);
                triangulate(i + 1, indices[index + 2]);
            }
        }
    }

    // Construct the model, indices are already generated
    u::vector<m::vec3> positions_(count);
    u::vector<m::vec3> normals_(count);
    u::vector<m::vec3> coordinates_(count);
    for (auto &it : uniques) {
        const auto &first = it.first;
        const auto &second = it.second;
        positions_[second] = vertices[first.vertex];
        if (normals.size())
            normals_[second] = normals[first.normal];
        if (coordinates.size())
            coordinates_[second] = coordinates[first.coordinate];
    }

    // Optimize the indices (on a per group basis)
    vertexCacheOptimizer vco;
    vco.optimize(indices);

    // Change winding order
    for (size_t i = 0; i < indices.size(); i += 3)
        u::swap(indices[i], indices[i + 2]);

    // Calculate tangents
    u::vector<m::vec3> tangents_(count);
    u::vector<float> bitangents_(count);
    createTangents(positions_, coordinates_, normals_, indices, tangents_, bitangents_);

    u::unique_ptr<mesh> mesh_(new mesh);

    // Calculate bounding box
    for (const auto &it : positions_)
        mesh_->m_bounds.expand(it);

    // Interleave for GPU
    mesh_->m_vertices.resize(count);
    for (size_t i = 0; i < count; i++) {
        auto &vert = mesh_->m_vertices[i];
        for (size_t j = 0; j < 3; j++) {
            vert.position[j] = positions_[i][j];
            vert.normal[j] = normals_[i][j];
            vert.tangent[j] = tangents_[i][j];
        }
        vert.coordinate[0] = coordinates_[i].x;
        vert.coordinate[1] = coordinates_[i].y;
        vert.tangent[3] = bitangents_[i];
    }

    // size_t -> unsigned int
    mesh_->m_indices.reserve(count);
    for (auto &it : indices)
        mesh_->m_indices.push_back(it);

    meshes.push_back(mesh_.release());
    return true;
}

model::~model() {
    for (auto it : m_meshes)
        delete it;
}

bool model::load(const u::string &file) {
    obj o;
    if (!o.load(file, m_meshes))
        return false;

    // Copy the vertex and index data while generating batches for rendering
    batch b;
    for (auto it : m_meshes) {
        b.index = m_indices.size();
        b.count = it->m_indices.size();
        const size_t index = m_vertices.size();
        const size_t count = it->m_vertices.size();
        m_vertices.resize(index + count);
        memcpy(&m_vertices[index], &it->m_vertices[0], sizeof(mesh::vertex)*count);
        m_indices.resize(b.index + b.count);
        memcpy(&m_indices[b.index], &it->m_indices[0], sizeof(unsigned int)*b.count);
        m_batches.push_back(b);
        // Also adjust bounding box
        m_bounds.expand(it->m_bounds);
    }

    return true;
}
