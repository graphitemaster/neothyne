#ifndef R_AA_HDR
#define R_AA_HDR
#include "r_common.h"
#include "r_method.h"

namespace m {
    struct mat4;
    struct perspective;
}

namespace r {

struct aa {
    aa();
    ~aa();

    bool init(const m::perspective &p);

    void update(const m::perspective &p);
    void bindWriting();

    GLuint texture() const;

private:
    void destroy();

    GLuint m_fbo;
    GLuint m_texture;
    size_t m_width;
    size_t m_height;
};

}

#endif
