#ifndef R_METHOD_HDR
#define R_METHOD_HDR
#include "r_common.h"

#include "u_string.h"
#include "u_vector.h"
#include "u_optional.h"
#include "u_map.h"
#include "u_eon.h"

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

    static u::optional<type> typeFromString(const char *type);

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

    // Note: the former function does not establish the type information
    // for the uniform. When registering rendering methods for the first
    // time the latter variant must be used to establish the type with
    // the uniform.
    uniform *getUniform(const u::string &name);
    uniform *getUniform(const u::string &name, uniform::type type);

protected:
    friend struct methods;

    bool addShader(GLenum shaderType, const char *shaderText);

    bool finalize(const u::vector<const char *> &attributes = {},
                  const u::vector<const char *> &fragData = {},
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
    u::vector<const char *> m_attributes;
    u::vector<const char *> m_fragData;
    GLuint m_program;
    const char *m_description;
    u::vector<u::string> m_defines;
};

struct methods {
    static methods &instance() {
        return m_instance;
    }

    bool init();
    void release();
    bool reload();
    method &operator[](const u::string &name);
    const method &operator[](const u::string &name) const;

protected:
    bool readMethod(const u::eon::entry *e);
private:
    methods();
    methods(const methods &) = delete;
    void operator =(const methods &) = delete;

    u::map<u::string, method> *m_methods;
    bool m_initialized;
    static methods m_instance;
};

inline method::shader::shader()
    : shaderFile(nullptr)
    , object(0)
{
}

}

#endif
