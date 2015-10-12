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

    uniform();
    uniform(type t);

    void set(int value);                        // kInt
    void set(int x, int y);                     // kInt2
    void set(float value);                      // kFloat
    void set(const m::vec2 &value);             // kVec2
    void set(const m::vec3 &value);             // kVec3
    void set(const m::vec4 &value);             // kVec4
    void set(size_t count, const float *mats);  // kMat3x4Array
    void set(const m::mat4 &value);             // kMat4

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
        m::mat4 asMat4;
        struct {
            float *data;
            size_t count;
        } asMat3x4Array;
    };
    GLint m_handle;
};

inline uniform::uniform(type t)
    : uniform()
{
    m_type = t;
    m_handle = -1;
}

struct method {
    static constexpr size_t kMat3x4Space = 80;

    method();
    ~method();

    void enable();
    void destroy();

    bool init(const char *description = "unknown");
    bool reload();

    void define(const char *string);
    void define(const char *string, size_t value);
    void define(const char *string, float value);

    uniform *getUniform(const u::string &name, uniform::type type);

protected:
    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize(const u::initializer_list<const char *> &attributes = {},
                  const u::initializer_list<const char *> &fragData = {},
                  bool initial = true);

    u::optional<u::string> preprocess(const u::string &file, bool initial = true);

    void post();

private:
    static m::mat3x4 m_mat3x4Scratch[kMat3x4Space];

    struct shader {
        shader();
        const char *shaderFile;
        u::string shaderText;
        GLuint object;
    };

    u::string m_prelude;
    u::map<GLenum, shader> m_shaders;
    u::map<u::string, uniform> m_uniforms;
    u::initializer_list<const char *> m_attributes;
    u::initializer_list<const char *> m_fragData;
    GLuint m_program;
    const char *m_description;
    u::vector<u::string> m_defines;
};

inline method::shader::shader()
    : shaderFile(nullptr)
    , object(0)
{
}

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

struct bboxMethod : method {
    bboxMethod();

    bool init();

    void setWVP(const m::mat4 &wvp);
    void setColor(const m::vec3 &color);

private:
    uniform *m_WVP;
    uniform *m_color;
};

inline bboxMethod::bboxMethod()
    : m_WVP(nullptr)
    , m_color(nullptr)
{
}

}

#endif
