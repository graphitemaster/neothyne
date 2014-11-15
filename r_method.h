#ifndef R_METHOD_HDR
#define R_METHOD_HDR
#include "r_common.h"

#include "u_string.h"
#include "u_vector.h"
#include "u_optional.h"

namespace r {

struct method {
    method();
    ~method();

    void enable();

    bool init();

    void define(const u::string &string);
    void define(const u::string &string, size_t value);
    void define(const u::string &string, float value);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize();

    u::optional<u::string> preprocess(const u::string &file);

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
