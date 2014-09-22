#ifndef R_METHOD_HDR
#define R_METHOD_HDR
#include "r_common.h"

#include "util.h"

namespace r {

struct method {
    method();
    ~method();

    void enable(void);

    bool init(void);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize(void);

    GLint getUniformLocation(const char *name);
    GLint getUniformLocation(const u::string &name);

private:
    GLuint m_program;
    u::string m_vertexSource;
    u::string m_fragmentSource;
    u::string m_geometrySource;
    u::vector<GLuint> m_shaders;
};

}

#endif
