#include "engine.h"

#include "r_method.h"

#include "u_file.h"
#include "u_misc.h"
#include "u_sha512.h"
#include "u_eon.h"

namespace r {

///! uniform
uniform::uniform() {
    memset(this, 0, sizeof *this);
}

void uniform::set(int value) {
    u::assert(m_type == kInt);
    asInt = value;
    post();
}

void uniform::set(int x, int y) {
    u::assert(m_type == kInt2);
    asInt2[0] = x;
    asInt2[1] = y;
    post();
}

void uniform::set(float value) {
    u::assert(m_type == kFloat);
    asFloat = value;
    post();
}

void uniform::set(const m::vec2 &value) {
    u::assert(m_type == kVec2);
    asVec2 = value;
    post();
}

void uniform::set(const m::vec3 &value) {
    u::assert(m_type == kVec3);
    asVec3 = value;
    post();
}

void uniform::set(const m::vec4 &value) {
    u::assert(m_type == kVec4);
    asVec4 = value;
    post();
}

void uniform::set(const m::mat4 &value) {
    u::assert(m_type == kMat4);
    asMat4 = value;
    post();
}

void uniform::set(size_t count, const float *mats) {
    u::assert(m_type == kMat3x4Array);
    u::assert(count <= method::kMat3x4Space);

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
        gl::UniformMatrix3x4fv(m_handle, asMat3x4Array.count, GL_FALSE, asMat3x4Array.data);
        break;
    case kMat4:
        gl::UniformMatrix4fv(m_handle, 1, GL_TRUE, asMat4.ptr());
        break;
    }
}

///! methodCacheHeader
struct methodCacheHeader {
    static constexpr const char *kMagic = "SHADER";
    static constexpr uint16_t kVersion = 1;
    char magic[6];
    uint16_t version;
    uint32_t format;
    void endianSwap();
};

inline void methodCacheHeader::endianSwap() {
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

u::optional<u::string> method::preprocess(const u::string &file, bool initial) {
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
                const u::optional<u::string> include = preprocess(thing, false);
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
            const auto find = name.find('[');
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
    const auto pp = preprocess(shaderFile);
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

uniform *method::getUniform(const u::string &name) {
    return &m_uniforms[name];
}

uniform *method::getUniform(const u::string &name, uniform::type type) {
    auto *const value = &m_uniforms[name];
    value->m_type = type;
    if (type == uniform::kMat3x4Array)
        value->asMat3x4Array.data = (float *)&m_mat3x4Scratch[0];
    return value;
}

bool method::finalize(const u::vector<const char *> &attributes,
                      const u::vector<const char *> &fragData,
                      bool initial)
{
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
        auto hash = u::sha512((const unsigned char *)&concatenate[0],
                              concatenate.size());
        // Does this program exist?
        const u::string cacheString = u::format("cache/shaders/%s", hash.hex());
        const u::string file = neoUserPath() + cacheString;
        if (!u::exists(file))
            break;
        // Load the cached program in memory
        auto load = u::read(file, "rb");
        if (!load)
            break;
        methodCacheHeader *header = (methodCacheHeader*)&(*load)[0];
        header->endianSwap();
        if (memcmp(header->magic, (const void *)methodCacheHeader::kMagic, sizeof header->magic)
            || header->version != methodCacheHeader::kVersion)
        {
            u::remove(file);
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
                u::print("[cache] => loaded %.50s...\n", u::fixPath(cacheString));
                notUsingCache = false;
            }
            break;
        }
        // Not supported, remove it
        u::print("[cache] => failed loading `%s'\n", u::fixPath(cacheString));
        u::remove(file);
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
            auto hash = u::sha512((const unsigned char *const)&concatenate[0],
                                  concatenate.size());
            const u::string cacheString = u::format("cache/shaders/%s", hash.hex());
            const u::string file = neoUserPath() + cacheString;

            // Get the program binary from the driver
            GLint length = 0;
            GLenum binaryFormat = 0;
            gl::GetProgramiv(m_program, GL_PROGRAM_BINARY_LENGTH, &length);
            u::vector<unsigned char> programBinary(length);
            gl::GetProgramBinary(m_program, length, nullptr, &binaryFormat, &programBinary[0]);

            // Create the header
            methodCacheHeader header;
            memcpy(header.magic, (const void *)methodCacheHeader::kMagic, 6);
            header.version = methodCacheHeader::kVersion;
            header.format = binaryFormat;
            header.endianSwap();

            // Serialize the data
            u::vector<unsigned char> serialize(sizeof header + programBinary.size());
            memcpy(&serialize[0], &header, sizeof header);
            memcpy(&serialize[sizeof header], &programBinary[0], programBinary.size());

            u::write(serialize, file, "wb");
            u::print("[cache] => wrote %.50s...\n", u::fixPath(cacheString));
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

///! Singleton representing all the possible rendering methods
methods::methods()
    : m_methods(nullptr)
    , m_initialized(false)
{
}

void methods::release() {
    delete m_methods;
}

bool methods::init() {
    if (m_initialized)
        return true;
    m_methods = new u::map<u::string, method>;
    const auto file = u::fixPath(neoGamePath() + "methods.eon");
    auto read = u::read(file, "r");
    if (!read) {
        u::print("[eon] => `%s' failed to read method description\n", file);
        return false;
    }

    // Read the EON
    const auto &data = *read;
    u::eon eon;
    if (!eon.load((const char *)&data[0], (const char *)&data[0] + data.size()))
        return false;

    auto readMethod = [&](const u::eon::entry *e) {
        const u::eon::entry *head = e;
        // Find the method
        const char *name = nullptr;
        for (; e; e = e->next) {
            if (strcmp(e->name, "name"))
                continue;
            name = e->head->name;
            break;
        }
        if (name) {
            u::print("[eon] => loading `%s' rendering method\n", name);
            method *m = &(*m_methods)[name];
            bool hasVertex = false;
            bool hasFragment = false;
            if (!m->init(name))
                return false;
            if (gl::has(gl::ARB_texture_rectangle))
                m->define("HAS_TEXTURE_RECTANGLE");
            for (e = head; e; e = e->next) {
                if (!strcmp(e->name, "name")
                ||  !strcmp(e->name, "attributes")
                ||  !strcmp(e->name, "fragdata")
                ||  !strcmp(e->name, "uniforms"))
                {
                    // Attributes and fragment data are expected to be done
                    // during the finalizer stage, so we skip those and
                    // intend to pick them up later.
                    // The name field is required to be first when finding
                    // the method. So it's already been processed. We skip
                    // that here too.
                    // Uniforms must be fetched after finalizing
                    continue;
                } else if (!strcmp(e->name, "vertex")) {
                    if (hasVertex) {
                        u::print("[eon] => method `%s' already has vertex shader\n", name);
                        return false;
                    }
                    if (!m->addShader(GL_VERTEX_SHADER, e->head->name))
                        return false;
                    hasVertex = true;
                } else if (!strcmp(e->name, "fragment")) {
                    if (hasFragment) {
                        u::print("[eon] => method `%s' already has fragment shader\n", name);
                        return false;
                    }
                    if (!m->addShader(GL_FRAGMENT_SHADER, e->head->name))
                        return false;
                    hasFragment = true;
                } else {
                    u::print("[eon] => method `%s' contains invalid field `%s'\n",
                        name, e->name);
                    return false;
                }
            }
            u::vector<const char *> attributes;
            u::vector<const char *> fragData;
            for (e = head; e; e = e->next) {
                if (!strcmp(e->name, "attributes")) {
                    for (const u::eon::entry *a = e->head; a; a = a->next)
                        attributes.push_back(u::string(a->name).copy());
                } else if (!strcmp(e->name, "fragdata")) {
                    for (const u::eon::entry *f = e->head; f; f = f->next)
                        fragData.push_back(u::string(f->name).copy());
                }
            }
            if (!m->finalize(attributes, fragData))
                return false;
            // Now fetch the uniforms
            for (e = head; e; e = e->next) {
                if (strcmp(e->name, "uniforms"))
                    continue;
                auto uniformType = [](const char *type) -> int {
                    if (!strcmp(type, "sampler"))
                        return uniform::kSampler;
                    else if (!strcmp(type, "float"))
                        return uniform::kFloat;
                    else if (!strcmp(type, "vec2"))
                        return uniform::kVec2;
                    else if (!strcmp(type, "vec3"))
                        return uniform::kVec3;
                    else if (!strcmp(type, "mat3x4[]"))
                        return uniform::kMat3x4Array;
                    else if (!strcmp(type, "int2"))
                        return uniform::kInt2;
                    else if (!strcmp(type, "mat4"))
                        return uniform::kMat4;
                    return -1;
                };
                for (const u::eon::entry *uniform = e->head; uniform; uniform = uniform->next) {
                    if (uniform->head->name) {
                        int type = uniformType(uniform->head->name);
                        if (type == -1) {
                            u::print("[eon] => method `%s' defines uniform `%s' with invalid type `%s'\n",
                                name, uniform->name, uniform->head->name);
                            return false;
                        }
                        m->getUniform(uniform->name, uniform::type(uniformType(uniform->head->name)));
                        continue;
                    }
                    u::print("[eon] => method `%s' defines uniform `%s' with no type\n",
                        name, uniform->name);
                    return false;
                }
            }
            // Post the uniforms
            m->post();
            return true;
        }
        u::print("[eon] => invalid method description\n");
        return false;
    };

    auto readRoot = [&readMethod](const u::eon::entry *e) {
        if (e->type != u::eon::kSection)
            return false;
        if (!strcmp(e->name, "method")) {
            if (!readMethod(e->head))
                return false;
            return true;
        }
        return false;
    };

    for (u::eon::entry *current = eon.root()->head; current; current = current->next)
        if (!readRoot(current))
            return false;

    return m_initialized = true;
}

const method &methods::operator[](const u::string &name) const {
    return (*m_methods)[name];
}

method &methods::operator[](const u::string &name) {
    return (*m_methods)[name];
}

bool methods::reload() {
    for (auto &it : *m_methods) {
        if (it.second.reload())
            continue;
        return false;
    }
    return true;
}

methods methods::m_instance;

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
