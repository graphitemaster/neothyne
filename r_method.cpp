#include "engine.h"
#include "r_method.h"
#include "u_file.h"
#include "u_misc.h"

namespace r {

method::method()
    : m_program(0)
    , m_vertexSource("#version 330 core\n")
    , m_fragmentSource("#version 330 core\n")
    , m_geometrySource("#version 330 core\n")
{
}

method::~method() {
    for (auto &it : m_shaders)
        gl::DeleteShader(it);
    if (m_program)
        gl::DeleteProgram(m_program);
}

bool method::init() {
    m_program = gl::CreateProgram();
    return !!m_program;
}

void method::define(const u::string &macro) {
    auto prelude = u::format("#define %s\n", macro);
    m_vertexSource += prelude;
    m_fragmentSource += prelude;
    m_geometrySource += prelude;
}

void method::define(const u::string &macro, size_t value) {
    auto prelude = u::format("#define %s %zu\n", macro, value);
    m_vertexSource += prelude;
    m_fragmentSource += prelude;
    m_geometrySource += prelude;
}

void method::define(const u::string &macro, float value) {
    auto prelude = u::format("#define %s %f\n", macro, value);
    m_vertexSource += prelude;
    m_fragmentSource += prelude;
    m_geometrySource += prelude;
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

bool method::addShader(GLenum shaderType, const char *shaderFile) {
    u::string *shaderSource = nullptr;
    switch (shaderType) {
        case GL_VERTEX_SHADER:
            shaderSource = &m_vertexSource;
            break;
        case GL_FRAGMENT_SHADER:
            shaderSource = &m_fragmentSource;
            break;
        case GL_GEOMETRY_SHADER:
            shaderSource = &m_geometrySource;
            break;
        default:
            break;
    }

    auto pp = preprocess(shaderFile);
    if (!pp)
        neoFatal("failed preprocessing `%s'", shaderFile);

    *shaderSource += *pp;

    GLuint shaderObject = gl::CreateShader(shaderType);
    if (!shaderObject)
        return false;

    m_shaders.push_back(shaderObject);

    const GLchar *shaderSources[1];
    GLint shaderLengths[1];

    shaderSources[0] = (GLchar *)&(*shaderSource)[0];
    shaderLengths[0] = (GLint)shaderSource->size();

    gl::ShaderSource(shaderObject, 1, shaderSources, shaderLengths);
    gl::CompileShader(shaderObject);

    GLint shaderCompileStatus = 0;
    gl::GetShaderiv(shaderObject, GL_COMPILE_STATUS, &shaderCompileStatus);
    if (shaderCompileStatus == 0) {
        u::string infoLog;
        GLint infoLogLength = 0;
        gl::GetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog.resize(infoLogLength);
        gl::GetShaderInfoLog(shaderObject, infoLogLength, nullptr, &infoLog[0]);
        u::print("shader compilation error `%s':\n%s\n", shaderFile, infoLog);
        return false;
    }

    gl::AttachShader(m_program, shaderObject);
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

bool method::finalize() {
    GLint success = 0;
    gl::LinkProgram(m_program);
    gl::GetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success)
        return false;

    gl::ValidateProgram(m_program);
    gl::GetProgramiv(m_program, GL_VALIDATE_STATUS, &success);
    if (!success)
        return false;

    for (auto &it : m_shaders)
        gl::DeleteShader(it);

    m_shaders.clear();
    return true;
}

}
