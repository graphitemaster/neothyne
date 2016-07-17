#include "engine.h"

#include "r_method.h"

#include "u_file.h"
#include "u_misc.h"
#include "u_hash.h"
#include "u_zip.h"
#include "u_set.h"

namespace r {

///! uniform
uniform::uniform() {
    memset(this, 0, sizeof *this);
}

void uniform::set(int value) {
    U_ASSERT(m_type == kInt);
    asInt = value;
    post();
}

void uniform::set(int x, int y) {
    U_ASSERT(m_type == kInt2);
    asInt2[0] = x;
    asInt2[1] = y;
    post();
}

void uniform::set(float value) {
    U_ASSERT(m_type == kFloat);
    asFloat = value;
    post();
}

void uniform::set(const m::vec2 &value) {
    U_ASSERT(m_type == kVec2);
    asVec2 = value;
    post();
}

void uniform::set(const m::vec3 &value) {
    U_ASSERT(m_type == kVec3);
    asVec3 = value;
    post();
}

void uniform::set(const m::vec4 &value) {
    U_ASSERT(m_type == kVec4);
    asVec4 = value;
    post();
}

void uniform::set(const m::mat4 &value) {
    U_ASSERT(m_type == kMat4);
    asMat4 = value;
    post();
}

void uniform::set(size_t count, const float *mats) {
    U_ASSERT(m_type == kMat3x4Array);
    U_ASSERT(count <= method::kMat3x4Space);

    memcpy(asMat3x4Array.data, mats, sizeof(m::mat3x4) * count);
    asMat3x4Array.count = count;
    post();
}

void uniform::post() {
    // Can't post change because the uniform handle is invalid. This is likely the
    // result of using a uniform that is not found. This is allowed by the engine
    // to make shader permutations less annoying to deal with. We waste some memory
    // storing otherwise unpostable uniforms in some cases, but so be it.
    if (m_handle == -1)
        return;
    switch (m_type) {
    case kInt:
        gl::Uniform1i(m_handle, asInt);
        break;
    case kInt2:
        gl::Uniform2i(m_handle, asInt2[0], asInt2[1]);
        break;
    case kFloat:
        gl::Uniform1f(m_handle, asFloat);
        break;
    case kVec2:
        gl::Uniform2fv(m_handle, 1, &asVec2.x);
        break;
    case kVec3:
        gl::Uniform3fv(m_handle, 1, &asVec3.x);
        break;
    case kVec4:
        gl::Uniform4fv(m_handle, 1, &asVec4.x);
        break;
    case kMat3x4Array:
        gl::UniformMatrix3x4fv(m_handle, asMat3x4Array.count, GL_FALSE, (const GLfloat *)asMat3x4Array.data);
        break;
    case kMat4:
        gl::UniformMatrix4fv(m_handle, 1, GL_TRUE, asMat4.ptr());
        break;
    }
}

///! MethodCacheHeader
struct MethodCacheHeader {
    static constexpr const char *kMagic = "SHADER";
    static constexpr uint16_t kVersion = 1;
    char magic[6];
    uint16_t version;
    uint32_t format;
    void endianSwap();
};

inline void MethodCacheHeader::endianSwap() {
    version = u::endianSwap(version);
    format = u::endianSwap(format);
}


///! method
m::mat3x4 method::m_mat3x4Scratch[method::kMat3x4Space];

method::method()
    : m_program(0)
    , m_description(nullptr)
{
}

method::~method() {
    destroy();
}

void method::destroy() {
    for (auto &it : m_shaders)
        if (it.second.object)
            gl::DeleteShader(it.second.object);
    if (m_program)
        gl::DeleteProgram(m_program);
}

bool method::init(const char *description) {
    m_program = gl::CreateProgram();
    m_prelude = u::move(u::format("#version %d\n", gl::glslVersion()));
    m_description = description;
    return !!m_program;
}

bool method::reload() {
    gl::UseProgram(0);
    destroy();
    if (!(m_program = gl::CreateProgram()))
        return false;
    for (const auto &it : m_shaders)
        if (!addShader(it.first, it.second.shaderFile))
            return false;
    if (!finalize(m_attributes, m_fragData, false))
        return false;
    post();
    enable();
    for (auto &it : m_uniforms)
        it.second.post();
    return true;
}

void method::define(const char *macro) {
    m_prelude += u::format("#define %s\n", macro);
    // keep a list of "feature" macros
    if (!strncmp(macro, "HAS_", 3) ||
        !strncmp(macro, "USE_", 3))
        m_defines.push_back(macro + 4); // skip "HAS_" or "USE_"
}

void method::define(const char *macro, size_t value) {
    // keep a list of defines
    m_prelude += u::format("#define %s %zu\n", macro, value);
}

void method::define(const char *macro, float value) {
    // keep a list of defines
    m_prelude += u::format("#define %s %f\n", macro, value);
}

u::optional<u::string> method::preprocess(const u::string &file, u::set<u::string> &uniforms, bool initial) {
    auto fp = u::fopen(neoGamePath() + file, "r");
    if (!fp)
        return u::none;
    u::string result;
    if (initial)
        result = m_prelude;
    size_t lineno = 1;
    for (u::string line; u::getline(fp, line); ) {
        while (line[0] && strchr(" \t", line[0])) line.pop_front();
        if (line[0] == '#') {
            const auto split = u::split(&line[1]);
            if (split.size() == 2 && split[0] == "include") {
                auto thing = split[1];
                const auto front = thing.pop_front(); // '"' or '<'
                const auto back = thing.pop_back(); // '"' or '>'
                if ((front == '<' && back != '>') && (front != back))
                    return u::format("#error invalid use of include directive on line %zu\n", lineno);
                const u::optional<u::string> include = preprocess(thing, uniforms, false);
                if (!include)
                    return u::format("#error failed to include %s\n", thing);
                result += u::format("#line %zu\n%s\n", lineno++, *include);
                continue;
            }
        }
        if (!strncmp(&line[0], "uniform", 7)) {
            // don't emit the uniform line if it's already declared
            u::string name = &line[0] + 7;
            while (u::isspace(name[0])) name.pop_front();
            size_t semicolon = name.find(';');
            if (semicolon != u::string::npos)
                name.erase(semicolon, semicolon);
            if (uniforms.find(name) == uniforms.end()) {
                uniforms.insert(name);
                result += line + "\n";
            }
        } else {
            result += line + "\n";
        }
        lineno++;
    }
    return result;
}

bool method::addShader(GLenum type, const char *shaderFile) {
    u::set<u::string> uniforms;
    const auto pp = preprocess(shaderFile, uniforms);
    if (!pp)
        neoFatal("failed preprocessing `%s'", shaderFile);
    auto &what = m_shaders[type];
    what.shaderFile = shaderFile;
    what.shaderText = u::move(*pp);

    return true;
}

void method::enable() {
    gl::UseProgram(m_program);
}

void method::post() {
    // Lookup all uniform locations
    for (auto &it : m_uniforms)
        it.second.m_handle = gl::GetUniformLocation(m_program, it.first.c_str());
}

uniform *method::getUniform(const u::string &name, uniform::type type) {
    auto *const value = &m_uniforms[name];
    value->m_type = type;
    if (type == uniform::kMat3x4Array)
        value->asMat3x4Array.data = m_mat3x4Scratch;
    return value;
}

static u::zip gMethodCache;

static bool mountCache() {
    if (gMethodCache.opened())
        return true;
    const auto cacheFile = neoUserPath() + "cache/methods.zip";
    if (u::exists(cacheFile)) {
        // Cache already exists: open
        if (!gMethodCache.open(cacheFile))
            return false;
        u::print("[cache] => mounted method cache `%s'\n", cacheFile);
        return true;
    }
    // Cache does not exist: create the file
    if (!gMethodCache.create(cacheFile))
        return false;
    u::print("[cache] => created method cache `%s'\n", cacheFile);
    return true;
}

bool method::finalize(const u::initializer_list<const char *> &attributes,
                      const u::initializer_list<const char *> &fragData,
                      bool initial)
{
    if (!mountCache())
        return false;

    // Check if the system supports any program binary format
    GLint formats = 0;
    if (gl::has(gl::ARB_get_program_binary))
        gl::GetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &formats);

    // Before compiling the shaders, concatenate their text and hash their contents.
    // We'll then check if there exists a cached copy of this program and load
    // that instead.
    bool notUsingCache = true;
    while (formats) {
        u::string concatenate;
        // Calculate how much memory we need to concatenate the shaders
        size_t size = 0;
        for (const auto &it : m_shaders)
            size += it.second.shaderText.size();
        concatenate.reserve(size);
        // Concatenate the shaders
        for (const auto &it : m_shaders)
            concatenate += it.second.shaderText;
        // Hash the result of the concatenation
        auto hash = u::djbx33a((const unsigned char *)&concatenate[0], concatenate.size());
        // Load the cached program in memory
        auto load = gMethodCache.read(hash.hex());
        if (!load)
            break;
        MethodCacheHeader *header = (MethodCacheHeader*)&(*load)[0];
        header->endianSwap();
        if (memcmp(header->magic, (const void *)MethodCacheHeader::kMagic, sizeof header->magic)
            || header->version != MethodCacheHeader::kVersion)
        {
            gMethodCache.remove(hash.hex());
            break;
        }

        // Check if the format is supported
        u::vector<GLint> supportedFormats(formats);
        gl::GetIntegerv(GL_PROGRAM_BINARY_FORMATS, &supportedFormats[0]);
        if (u::find(supportedFormats.begin(), supportedFormats.end(), GLint(header->format)) != supportedFormats.end()) {
            GLint status = 0;
            // Use the program Binary
            gl::ProgramBinary(m_program, header->format, &(*load)[sizeof *header], (*load).size() - sizeof *header);
            // Verify that this program is valid (linked)
            gl::GetProgramiv(m_program, GL_LINK_STATUS, &status);
            if (status) {
                u::print("[cache] => loaded (method) %s\n", hash.hex());
                notUsingCache = false;
            }
            break;
        }
        // Not supported, remove it
        u::print("[cache] => failed loading `%s'\n", hash.hex());
        gMethodCache.remove(hash.hex());
        break;
    }

    if (notUsingCache) {
        // compile the individual shaders
        auto compile = [this](GLenum type, shader &shader_) {
            GLint size = shader_.shaderText.size(),
                  status = 0,
                  length = 0;

            if (!(shader_.object = gl::CreateShader(type)))
                return false;

            const GLchar *source = &shader_.shaderText[0];
            gl::ShaderSource(shader_.object, 1, &source, &size);
            gl::CompileShader(shader_.object);
            gl::GetShaderiv(shader_.object, GL_COMPILE_STATUS, &status);

            if (status) {
                gl::AttachShader(m_program, shader_.object);
                return true;
            }

            gl::GetShaderiv(shader_.object, GL_INFO_LOG_LENGTH, &length);

            u::vector<char> log(length+1);
            gl::GetShaderInfoLog(shader_.object, length, nullptr, &log[0]);
            u::print("shader compilation error `%s':\n%s\n%s",
                     shader_.shaderFile,
                     &log[0],
                     source);
            return false;
        };
        for (auto &it : m_shaders)
            compile(it.first, it.second);

        // Check if compilation succeeded
        GLint success = 0;
        GLint infoLogLength = 0;
        u::string infoLog;

        // Setup attributes
        for (size_t i = 0; i < attributes.size(); i++)
            gl::BindAttribLocation(m_program, i, attributes[i]);
        for (size_t i = 0; i < fragData.size(); i++)
            gl::BindFragDataLocation(m_program, i, fragData[i]);

        if (gl::has(gl::ARB_get_program_binary)) {
            // Need to hint to the compiler that in the future we'll be needing
            // to retrieve the program binary
            gl::ProgramParameteri(m_program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
        }

        gl::LinkProgram(m_program);
        gl::GetProgramiv(m_program, GL_LINK_STATUS, &success);
        if (!success) {
            gl::GetProgramiv(m_program, GL_INFO_LOG_LENGTH, &infoLogLength);
            infoLog.resize(infoLogLength);
            gl::GetProgramInfoLog(m_program, infoLogLength, nullptr, &infoLog[0]);
            u::print("shader link error:\n%s\n", infoLog);
            return false;
        }

        if (formats) {
            // On some implementations, the binary is not actually available until the
            // program has been used.
            gl::UseProgram(m_program);

            u::string concatenate;
            // Calculate how much memory we need to concatenate the shaders
            size_t size = 0;
            for (auto &it : m_shaders)
                size += it.second.shaderText.size();
            concatenate.reserve(size);
            // Concatenate the shaders
            for (auto &it : m_shaders)
                concatenate += it.second.shaderText;

            // Hash the result of the concatenation
            auto hash = u::djbx33a((const unsigned char *const)&concatenate[0], concatenate.size());
            // Get the program binary from the driver
            GLint length = 0;
            GLenum binaryFormat = 0;
            gl::GetProgramiv(m_program, GL_PROGRAM_BINARY_LENGTH, &length);
            u::vector<unsigned char> programBinary(length);
            gl::GetProgramBinary(m_program, length, nullptr, &binaryFormat, &programBinary[0]);

            // Create the header
            MethodCacheHeader header;
            memcpy(header.magic, (const void *)MethodCacheHeader::kMagic, 6);
            header.version = MethodCacheHeader::kVersion;
            header.format = binaryFormat;
            header.endianSwap();

            // Serialize the data
            u::vector<unsigned char> serialize(sizeof header + programBinary.size());
            memcpy(&serialize[0], &header, sizeof header);
            memcpy(&serialize[sizeof header], &programBinary[0], programBinary.size());

            if (gMethodCache.write(hash.hex(), serialize))
                u::print("[cache] => wrote (method) %s\n", hash.hex());
            else
                return false;
        }

        // Don't need these anymore
        for (auto &it : m_shaders) {
            if (it.second.object) {
                gl::DeleteShader(it.second.object);
                it.second.object = 0;
            }
        }
    }

    // Make a copy of these for reloads
    m_attributes = attributes;
    m_fragData = fragData;

    // Make a pretty list of all the "USE_" and "HAS_" permutators for this
    // shader.
    u::string contents;
    for (size_t i = 0; i < m_defines.size(); i++) {
        contents += m_defines[i];
        if (i != m_defines.size() - 1)
            contents += ", ";
    }

    const char *const whence = initial ? "loaded" : "reloaded";
    if (contents.empty())
        u::print("[method] => %s `%s' program\n", whence, m_description);
    else
        u::print("[method] => %s `%s' program using [%s]\n", whence, m_description, contents);
    return true;
}

///! defaultMethod
defaultMethod::defaultMethod()
    : m_screenSize(nullptr)
    , m_colorTextureUnit(nullptr)
{
}

bool defaultMethod::init() {
    if (!method::init("default"))
        return false;

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    if (!addShader(GL_VERTEX_SHADER, "shaders/default.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/default.fs"))
        return false;
    if (!finalize({ "position" }))
        return false;

    m_screenSize = getUniform("gScreenSize", uniform::kVec2);
    m_colorTextureUnit = getUniform("gColorMap", uniform::kSampler);

    post();
    return true;
}

void defaultMethod::setColorTextureUnit(int unit) {
    m_colorTextureUnit->set(unit);
}

void defaultMethod::setPerspective(const m::perspective &p) {
    m_screenSize->set(m::vec2(p.width, p.height));
}

///! bboxMethod
bool bboxMethod::init() {
    if (!method::init("bounding box"))
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/bbox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/bbox.fs"))
        return false;
    if (!finalize({ "position" }, { "diffuseOut" }))
        return false;

    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_color = getUniform("gColor", uniform::kVec3);

    post();
    return true;
}

void bboxMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void bboxMethod::setColor(const m::vec3 &color) {
    m_color->set(color);
}

}
