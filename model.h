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
        size_t material; // Material index for rendering (calculated in r_model.cpp)
    };

    bool load(const u::string &file);

    const u::vector<mesh::basicVertex> &basicVertices() const;
    const u::vector<mesh::animVertex> &animVertices() const;
    const u::vector<unsigned int> &indices() const;
    const u::vector<batch> &batches() const;
    const u::vector<u::string> &meshNames() const;

    const m::bbox &bounds() const;

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

    // When loading OBJs the only mesh name this is populated with for now is
    // "default", as the OBJ loader only supports one mesh.
    // When loading IQMs however, this is populated with the names of the meshes
    // of the IQM is composed of. The IQM file must have mesh names, otherwise
    // this gets populated with a bunch of "default" strings.
    u::vector<u::string> m_meshNames;

    // these are only initialized when m_animated
    size_t m_numFrames; // number of frames
    size_t m_numJoints; // number of joints
    u::vector<m::mat3x4> m_frames; // frames
    u::vector<m::mat3x4> m_outFrame; // animated frames
    u::vector<int32_t> m_parents; // parent joint indices


    u::vector<mesh::animVertex> m_animVertices; // generated data for animated models
    u::vector<mesh::basicVertex> m_basicVertices; // generated data for unanimated models
};

inline model::model()
    : m_numFrames(0)
    , m_numJoints(0)
{
}

inline const u::vector<mesh::basicVertex> &model::basicVertices() const {
    return m_basicVertices;
}

inline const u::vector<mesh::animVertex> &model::animVertices() const {
    return m_animVertices;
}

inline const u::vector<unsigned int> &model::indices() const {
    return m_indices;
}

inline const u::vector<model::batch> &model::batches() const {
    return m_batches;
}

inline const u::vector<u::string> &model::meshNames() const {
    return m_meshNames;
}

inline const m::bbox &model::bounds() const {
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
