#ifndef MODEL_HDR
#define MODEL_HDR
#include "mesh.h"

struct model {
    ~model();

    struct batch {
        size_t index;
        size_t count;
    };

    bool load(const u::string &file);

    u::vector<mesh::vertex> vertices() const;
    u::vector<unsigned int> indices() const;
    u::vector<batch> batches() const;

    m::bbox bounds() const;

private:
    m::bbox m_bounds;
    u::vector<mesh*> m_meshes;
    u::vector<batch> m_batches;

    // Generated rendering data
    u::vector<mesh::vertex> m_vertices;
    u::vector<unsigned int> m_indices;
};

inline u::vector<mesh::vertex> model::vertices() const {
    return m_vertices;
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

#endif
