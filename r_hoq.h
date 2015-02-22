#ifndef R_HOQ_HDR
#define R_HOQ_HDR

#include "r_common.h"
#include "r_method.h"
#include "r_geom.h"

#include "u_vector.h"
#include "u_map.h"

#include "m_mat4.h"
#include "m_bbox.h"

namespace r {

struct occlusionMethod : method {
    occlusionMethod();
    bool init();
    void setWVP(const m::mat4 &wvp);
private:
    GLint m_WVPLocation;
};

inline occlusionMethod::occlusionMethod()
    : m_WVPLocation(-1)
{
}

struct occlusionQueries {
protected:
    bool begin();

    struct object {
        object(const m::mat4 &wvp, occlusionQueries *owner);

        object &operator=(const object &other);

    protected:
        void render(size_t index);

    private:
        friend struct occlusionQueries;
        m::mat4 m_wvp;
        occlusionQueries *m_owner;
        size_t m_index;
    };

public:
    occlusionQueries();

    bool init();

    void update();
    void render();

    void add(const m::mat4 &wvp, const void *id);

    bool passed(const void *id);

private:
    friend struct object;
    u::vector<object> m_objects; // Objects to query
    u::vector<GLuint> m_queries; // Occlusion query pool
    u::map<const void *, size_t> m_mapping; // Opaque object mapping with value = m_objects index
    int m_next; // Next available occlusion object
    int m_current; // Current occlusion object, -1 if linear-probe failed (occlusion query pool all in use.)
    occlusionMethod m_method; // Occlusion rendering method
    cube m_cube; // Cube geometry for occlusion bounding-box render
};

}

#endif
