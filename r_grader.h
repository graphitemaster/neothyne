#ifndef R_GRADER_HDR
#define R_GRADER_HDR
#include "r_common.h"

namespace m {

struct perspective;

}

namespace r {

struct grader {
    grader();
    ~grader();

    bool init(const m::perspective &p, const unsigned char *const colorGradingData);
    void update(const m::perspective &p, const unsigned char *const colorGradingData);
    void bindWriting(); // Also binds the color grading data

    enum type {
        kOutput,
        kColorGrading
    };

    GLuint texture(type what) const;

private:
    void destroy();

    GLuint m_fbo;
    GLuint m_textures[2];
    size_t m_width;
    size_t m_height;
};

}

#endif
