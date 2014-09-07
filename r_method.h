#ifndef R_METHOD_HDR
#define R_METHOD_HDR

#include "util.h"
#include "r_common.h"

struct method {
    method();
    ~method();

    void enable(void);

    bool init(void);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    void addVertexPrelude(const u::string &prelude);
    void addFragmentPrelude(const u::string &prelude);
    void addGeometryPrelude(const u::string &prelude);

    bool finalize(void);

    GLint getUniformLocation(const char *name);
    GLint getUniformLocation(const u::string &name);

private:
    GLuint m_program;
    u::string m_vertexSource;
    u::string m_fragmentSource;
    u::string m_geometrySource;
    u::list<GLuint> m_shaders;
};

#endif
