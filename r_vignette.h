#ifndef R_VIGNETTE_HDR
#define R_VIGNETTE_HDR
#include "r_common.h"
#include "r_method.h"

namespace r {

struct vignette {
    vignette();
    ~vignette();

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
