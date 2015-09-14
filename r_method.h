#ifndef R_METHOD_HDR
#define R_METHOD_HDR
#include "r_common.h"

#include "u_string.h"
#include "u_vector.h"
#include "u_optional.h"
#include "u_map.h"

#include "m_mat.h"

namespace m {

struct perspective;

}

namespace r {

struct uniform {
    enum type {
        kInt,
        kSampler = kInt,
        kInt2,
        kFloat,
        kVec2,
        kVec3,
        kVec4,
        kMat3x4Array,
        kMat4
    };

    uniform() {}
    uniform(type t);

    void set(int value);                                     // kInt
    void set(int x, int y);                                  // kInt2
    void set(float value);                                   // kFloat
    void set(const m::vec2 &value);                          // kVec2
    void set(const m::vec3 &value);                          // kVec3
    void set(const m::vec4 &value);                          // kVec4
    void set(size_t count, const float *mats);               // kMat3x4Array
    void set(const m::mat4 &value, bool transposed = true);  // kMat4

    void post();

private:
    friend struct method;

    type m_type;
    union {
        int asInt;
        int asInt2[2];
        float asFloat;
        m::vec2 asVec2;
        m::vec3 asVec3;
        m::vec4 asVec4;

        struct {
            bool transposed;
            m::mat4 data;
        } asMat4;

        struct {
            const float *data;
            size_t count;
        } asMat3x4Array;
    };
    GLint m_handle;
};

inline uniform::uniform(type t)
    : m_type(t)
    , m_handle(-1)
{
}

struct method {
    method();
    ~method();

    void enable();

    bool init();

    void define(const char *string);
    void define(const char *string, size_t value);
    void define(const char *string, float value);

    uniform *getUniform(const u::string &name, uniform::type type);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize(const u::initializer_list<const char *> &attributes = {},
                  const u::initializer_list<const char *> &fragData = {});

    u::optional<u::string> preprocess(const u::string &file);

    void post();

private:
    u::map<u::string, uniform> m_uniforms;

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
    uniform *m_WVP;
    uniform *m_screenSize;
    uniform *m_colorTextureUnit;
};

}

#endif
