#ifndef MODEL_HDR
#define MODEL_HDR
#include "mesh.h"

#include "m_mat.h"

#include "u_string.h"

struct model {
    model();
    ~model();

    struct batch {
        void *offset;
        size_t count;
        u::string name;
    };

    bool load(const u::string &file);

    u::vector<mesh::vertex> vertices() const;
    u::vector<mesh::animVertex> animVertices() const;
    u::vector<unsigned int> indices() const;
    u::vector<batch> batches() const;

    m::bbox bounds() const;

    bool animated() const;
    void animate(float curFrame);
    size_t joints() const;
    const float *bones() const;

private:
    friend struct obj;
    friend struct iqm;

    m::bbox m_bounds;
    u::vector<batch> m_batches;
    u::vector<unsigned int> m_indices;

    // these are only initialized when m_animated
    size_t m_numFrames; // number of frames (for animation only)
    size_t m_numJoints; // number of joints (for animation only)
    u::vector<m::mat3x4> m_frames; // frames
    u::vector<m::mat3x4> m_outFrame; // animated frames
    u::vector<int32_t> m_parents; // parent joint indices
    u::vector<mesh::animVertex> m_animVertices; // generated data for animated

    u::vector<mesh::vertex> m_vertices; // generated data for unanimated
};

inline model::model()
    : m_numFrames(0)
    , m_numJoints(0)
{
}

inline u::vector<mesh::vertex> model::vertices() const {
    return m_vertices;
}

inline u::vector<mesh::animVertex> model::animVertices() const {
    return m_animVertices;
}

inline u::vector<unsigned int> model::indices() const {
    return m_indices;
}

inline u::vector<model::batch> model::batches() const {
    return m_batches;
}

inline m::bbox model::bounds() const {
    return m_bounds;
}

inline bool model::animated() const {
    return m_numFrames;
}

inline const float *model::bones() const {
    return m_outFrame[0].a.m;
}

inline size_t model::joints() const {
    return m_numJoints;
}

#endif
