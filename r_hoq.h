#ifndef R_HOQ_HDR
#define R_HOQ_HDR

#include "r_common.h"
#include "r_method.h"
#include "r_geom.h"

#include "u_optional.h"
#include "u_vector.h"

#include "m_mat.h"

namespace r {

struct occlusionMethod : method {
    occlusionMethod();
    bool init();
    void setWVP(const m::mat4 &wvp);
private:
    uniform *m_WVP;
};

inline occlusionMethod::occlusionMethod()
    : m_WVP(nullptr)
{
}

struct occlusionQueries {
    typedef size_t ref;

protected:
    u::optional<ref> next() const;

    struct object {
        object(const m::mat4 &wvp, occlusionQueries *owner, ref handle);
    protected:
        void render();
    private:
        friend struct occlusionQueries;
        m::mat4 m_wvp;
        occlusionQueries *m_owner;
        ref m_handle; // Occlusion query handle
    };

public:
    occlusionQueries();

    bool init();

    void update();
    void render();

    // Add an object to do occlusion test
    u::optional<ref> add(const m::mat4 &wvp);

    // Check if an object passed the occlusion test
    bool passed(ref handle);

private:
    friend struct object;

    u::vector<object> m_objects; // Objects to query
    u::vector<GLuint> m_queries; // Occlusion object pool

    // Stores state of query objects:
    //   An unset bit indicates the query object is in use, while the inverse indicates
    //   availability. Linear probe for next available query object is done by finding
    //   the least-significant set bit in this.
    uint32_t m_bits;

    occlusionMethod m_method; // Occlusion rendering method
    cube m_cube; // Cube geometry for occlusion bounding-box render
};

}

#endif
