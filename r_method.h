#ifndef R_METHOD_HDR
#define R_METHOD_HDR
#include "r_common.h"

#include "u_string.h"
#include "u_vector.h"
#include "u_optional.h"

namespace m {

struct mat4;
struct perspective;

}

namespace r {

struct attribute {
    GLuint index;
    const char *name;
};

struct method {
    method();
    ~method();

    void enable();

    bool init();

    void define(const char *string);
    void define(const char *string, size_t value);
    void define(const char *string, float value);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize(const u::initializer_list<attribute> &attributes = {},
                  const u::initializer_list<attribute> &fragData = {});

    u::optional<u::string> preprocess(const u::string &file);

    GLint getUniformLocation(const char *name);
    GLint getUniformLocation(const u::string &name);

private:
    struct shader {
        shader();
        enum {
            kVertex,
            kFragment
        };
        u::string source;
        GLuint object;
    };
    shader m_shaders[2]; // shader::kVertex, shader::kFragment
    GLuint m_program;
};

struct defaultMethod : method {
    defaultMethod();
    bool init();
    void setColorTextureUnit(int unit);
    void setWVP(const m::mat4 &wvp);
    void setPerspective(const m::perspective &p);
private:
    GLint m_WVPLocation;
    GLint m_screenSizeLocation;
    GLint m_colorTextureUnitLocation;
};

}

#endif
