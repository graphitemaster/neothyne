#include "engine.h"

#include "r_method.h"

#include "u_file.h"
#include "u_misc.h"

#include "m_mat.h"

namespace r {

method::shader::shader()
    : object(0)
{
}

method::method()
    : m_program(0)
{
}

method::~method() {
    for (auto &it : m_shaders) {
        if (it.object)
            gl::DeleteShader(it.object);
    }
    if (m_program)
        gl::DeleteProgram(m_program);
}

bool method::init() {
    m_program = gl::CreateProgram();
    for (auto &it : m_shaders)
        it.source = u::move(u::format("#version %d\n", gl::glslVersion()));
    return !!m_program;
}

void method::define(const char *macro) {
    auto prelude = u::format("#define %s\n", macro);
    for (auto &it : m_shaders)
        it.source += prelude;
}

void method::define(const char *macro, size_t value) {
    auto prelude = u::format("#define %s %zu\n", macro, value);
    for (auto &it : m_shaders)
        it.source += prelude;
}

void method::define(const char *macro, float value) {
    auto prelude = u::format("#define %s %f\n", macro, value);
    for (auto &it : m_shaders)
        it.source += prelude;
}

u::optional<u::string> method::preprocess(const u::string &file) {
    auto fp = u::fopen(neoGamePath() + file, "r");
    if (!fp)
        return u::none;
    u::string result;
    size_t lineno = 1;
    while (auto read = u::getline(fp)) {
        auto line = *read;
        while (line[0] && strchr(" \t", line[0])) line.pop_front();
        if (line[0] == '#') {
            auto split = u::split(&line[1]);
            if (split.size() == 2 && split[0] == "include") {
                auto thing = split[1];
                auto front = thing.pop_front(); // '"' or '<'
                auto back = thing.pop_back(); // '"' or '>'
                if ((front == '<' && back != '>') && (front != back))
                    return u::format("#error invalid use of include directive on line %zu\n", lineno);
                u::optional<u::string> include = preprocess(thing);
                if (!include)
                    return u::format("#error failed to include %s\n", thing);
                result += u::format("#line %zu\n%s\n", lineno++, *include);
                continue;
            }
        }
        if (!strncmp(&line[0], "uniform", 7)) {
            // We put #ifdef uniform_name ... #define uniform_name #endif
            // around all uniforms to prevent declaring them twice. This is because
            // it's quite easy to cause multiple declarations when using headers
            auto split = u::split(line);
            split[2].pop_back(); // Remove the semicolon
            auto name = split[2];
            // Erase anything after '[', which would mark an array
            auto find = name.find('[');
            if (find != u::string::npos)
                name.erase(find, name.size());
            result += u::format("#ifndef uniform_%s\n"
                                "uniform %s %s;\n"
                                "#define uniform_%s\n"
                                "#endif\n"
                                "#line %zu\n",
                                name,
                                split[1], split[2],
                                name,
                                lineno);
        } else {
            result += line + "\n";
        }
        lineno++;
    }
    return result;
}

bool method::addShader(GLenum type, const char *shaderFile) {
    int index = -1;
    switch (type) {
    case GL_VERTEX_SHADER:
        index = shader::kVertex;
        break;
    case GL_FRAGMENT_SHADER:
        index = shader::kFragment;
        break;
    }
    assert(index != -1);

    auto pp = preprocess(shaderFile);
    if (!pp)
        neoFatal("failed preprocessing `%s'", shaderFile);

    GLuint object = gl::CreateShader(type);
    if (!object)
        return false;

    auto &entry = m_shaders[index];
    entry.source += *pp;
    entry.object = object;

    const auto &data = entry.source;
    const GLchar *source = &data[0];
    const GLint size = data.size();

    gl::ShaderSource(object, 1, &source, &size);
    gl::CompileShader(object);

    GLint status = 0;
    gl::GetShaderiv(object, GL_COMPILE_STATUS, &status);
    if (status == 0) {
        u::string log;
        GLint length = 0;
        gl::GetShaderiv(object, GL_INFO_LOG_LENGTH, &length);
        log.resize(length);
        gl::GetShaderInfoLog(object, length, nullptr, &log[0]);
        u::print("shader compilation error `%s':\n%s\n", shaderFile, log);
        return false;
    }

    gl::AttachShader(m_program, object);
    return true;
}

void method::enable() {
    gl::UseProgram(m_program);
}

GLint method::getUniformLocation(const char *name) {
    return gl::GetUniformLocation(m_program, name);
}

GLint method::getUniformLocation(const u::string &name) {
    return gl::GetUniformLocation(m_program, name.c_str());
}

bool method::finalize(const u::initializer_list<const char *> &attributes,
                      const u::initializer_list<const char *> &fragData)
{
    GLint success = 0;
    GLint infoLogLength = 0;
    u::string infoLog;

    for (size_t i = 0; i < attributes.size(); i++)
        gl::BindAttribLocation(m_program, i, attributes[i]);

    for (size_t i = 0; i < fragData.size(); i++)
        gl::BindFragDataLocation(m_program, i, fragData[i]);

    gl::LinkProgram(m_program);
    gl::GetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        gl::GetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog.resize(infoLogLength);
        gl::GetProgramInfoLog(m_program, infoLogLength, nullptr, &infoLog[0]);
        u::print("shader link error:\n%s\n", infoLog);
        return false;
    }

    // Don't need these anymore
    for (auto &it : m_shaders) {
        if (it.object) {
            gl::DeleteShader(it.object);
            it.object = 0;
        }
    }

    return true;
}

///! defaultMethod
defaultMethod::defaultMethod()
    : m_WVPLocation(-1)
    , m_screenSizeLocation(-1)
    , m_colorTextureUnitLocation(-1)
{
}

bool defaultMethod::init() {
    if (!method::init())
        return false;

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    if (!addShader(GL_VERTEX_SHADER, "shaders/default.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/default.fs"))
        return false;
    if (!finalize({ "position" }))
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");

    return true;
}

void defaultMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void defaultMethod::setWVP(const m::mat4 &wvp) {
    // Will set an identity matrix as this will be a screen space QUAD
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, wvp.ptr());
}

void defaultMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
}

}
