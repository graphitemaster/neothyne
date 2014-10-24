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

    auto source = u::read(neoGamePath() + shaderFile, "r");
    if (source)
        *shaderSource += u::string((*source).begin(), (*source).end());
    else
        return false;

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
        u::print("Shader error:\n%s\n", infoLog);
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
