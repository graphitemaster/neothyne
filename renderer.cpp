#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "renderer.h"

static constexpr size_t kScreenWidth = 1024;
static constexpr size_t kScreenHeight = 768;

static PFNGLCREATESHADERPROC             glCreateShader             = nullptr;
static PFNGLSHADERSOURCEPROC             glShaderSource             = nullptr;
static PFNGLCOMPILESHADERPROC            glCompileShader            = nullptr;
static PFNGLATTACHSHADERPROC             glAttachShader             = nullptr;
static PFNGLCREATEPROGRAMPROC            glCreateProgram            = nullptr;
static PFNGLLINKPROGRAMPROC              glLinkProgram              = nullptr;
static PFNGLUSEPROGRAMPROC               glUseProgram               = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation       = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray  = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv         = nullptr;
static PFNGLBINDBUFFERPROC               glBindBuffer               = nullptr;
static PFNGLGENBUFFERSPROC               glGenBuffers               = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer      = nullptr;
static PFNGLBUFFERDATAPROC               glBufferData               = nullptr;
static PFNGLVALIDATEPROGRAMPROC          glValidateProgram          = nullptr;
static PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays          = nullptr;
static PFNGLBINDVERTEXARRAYPROC          glBindVertexArray          = nullptr;
static PFNGLDELETEPROGRAMPROC            glDeleteProgram            = nullptr;
static PFNGLDELETEBUFFERSPROC            glDeleteBuffers            = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays       = nullptr;
static PFNGLUNIFORM1IPROC                glUniform1i                = nullptr;
static PFNGLUNIFORM1FPROC                glUniform1f                = nullptr;
static PFNGLUNIFORM3FPROC                glUniform3f                = nullptr;
static PFNGLGENERATEMIPMAPPROC           glGenerateMipmap           = nullptr;
static PFNGLDELETESHADERPROC             glDeleteShader             = nullptr;
static PFNGLGETSHADERIVPROC              glGetShaderiv              = nullptr;
static PFNGLGETPROGRAMIVPROC             glGetProgramiv             = nullptr;
static PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog         = nullptr;

static void checkError(const char *statement, const char *name, size_t line) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "GL error %08x, at %s:%zu - for %s\n", err, name, line,
            statement);
        abort();
    }
}

#define GL_CHECK(X) \
    do { \
        X; \
        checkError(#X, __FILE__, __LINE__); \
    } while (0)

///! rendererPipeline
rendererPipeline::rendererPipeline(void) :
    m_scale(1.0f, 1.0f, 1.0f)
{
    //
}

void rendererPipeline::scale(float scaleX, float scaleY, float scaleZ) {
    m_scale = m::vec3(scaleX, scaleY, scaleZ);
}

void rendererPipeline::position(float x, float y, float z) {
    m_position = m::vec3(x, y, z);
}

void rendererPipeline::rotate(float rotateX, float rotateY, float rotateZ) {
    m_rotate = m::vec3(rotateX, rotateY, rotateZ);
}

void rendererPipeline::setPerspectiveProjection(float fov, float width, float height, float near, float far) {
    m_projection.fov = fov;
    m_projection.width = width;
    m_projection.height = height;
    m_projection.near = near;
    m_projection.far = far;
}

void rendererPipeline::setCamera(const m::vec3 &position, const m::vec3 &target, const m::vec3 &up) {
    m_camera.position = position;
    m_camera.target = target;
    m_camera.up = up;
}

const m::mat4 &rendererPipeline::getWorldTransform(void) {
    m::mat4 scale, rotate, translate;
    scale.setScaleTrans(m_scale.x, m_scale.y, m_scale.z);
    rotate.setRotateTrans(m_rotate.x, m_rotate.y, m_rotate.z);
    translate.setTranslateTrans(m_position.x, m_position.y, m_position.z);

    m_worldTransform = translate * rotate * scale;
    return m_worldTransform;
}

const m::mat4 &rendererPipeline::getWVPTransform(void) {
    getWorldTransform();
    m::mat4 translate, rotate, perspective;
    translate.setTranslateTrans(-m_camera.position.x, -m_camera.position.y, -m_camera.position.z);
    rotate.setCameraTrans(m_camera.target, m_camera.up);
    perspective.setPersProjTrans(m_projection.fov, m_projection.width, m_projection.height,
        m_projection.near, m_projection.far);

    m_WVPTransform = perspective * rotate * translate * m_worldTransform;
    return m_WVPTransform;
}

const m::vec3 &rendererPipeline::getPosition(void) {
    return m_camera.position;
}

///! renderer
renderer::renderer(void) {
    once();

    m_light.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_light.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_light.ambient = 0.45f;
    m_light.diffuse = 0.75f;

    m_method.init();
}

renderer::~renderer(void) {
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
}

void renderer::draw(rendererPipeline &p) {
    const m::mat4 wvp = p.getWVPTransform();
    const m::mat4 &worldTransform = p.getWorldTransform();

    m_method.setWVP(wvp);
    m_method.setWorld(worldTransform);
    m_method.setLight(m_light);
    m_method.setEyeWorldPos(p.getPosition());
    m_method.setMatSpecIntensity(1.0f);
    m_method.setMatSpecPower(32.0f);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        m_textureBatches[i].tex.bind(GL_TEXTURE0);
        m_textureBatches[i].bump.bind(GL_TEXTURE1);
        glDrawElements(GL_TRIANGLES, m_textureBatches[i].count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(uint32_t) * m_textureBatches[i].start));
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDisableVertexAttribArray(3);
}

void renderer::once(void) {
    static bool onlyOnce = false;
    if (onlyOnce)
        return;

    *(void **)&glCreateShader             = SDL_GL_GetProcAddress("glCreateShader");
    *(void **)&glShaderSource             = SDL_GL_GetProcAddress("glShaderSource");
    *(void **)&glCompileShader            = SDL_GL_GetProcAddress("glCompileShader");
    *(void **)&glAttachShader             = SDL_GL_GetProcAddress("glAttachShader");
    *(void **)&glCreateProgram            = SDL_GL_GetProcAddress("glCreateProgram");
    *(void **)&glLinkProgram              = SDL_GL_GetProcAddress("glLinkProgram");
    *(void **)&glUseProgram               = SDL_GL_GetProcAddress("glUseProgram");
    *(void **)&glGetUniformLocation       = SDL_GL_GetProcAddress("glGetUniformLocation");
    *(void **)&glEnableVertexAttribArray  = SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    *(void **)&glDisableVertexAttribArray = SDL_GL_GetProcAddress("glDisableVertexAttribArray");
    *(void **)&glUniformMatrix4fv         = SDL_GL_GetProcAddress("glUniformMatrix4fv");
    *(void **)&glBindBuffer               = SDL_GL_GetProcAddress("glBindBuffer");
    *(void **)&glGenBuffers               = SDL_GL_GetProcAddress("glGenBuffers");
    *(void **)&glVertexAttribPointer      = SDL_GL_GetProcAddress("glVertexAttribPointer");
    *(void **)&glBufferData               = SDL_GL_GetProcAddress("glBufferData");
    *(void **)&glValidateProgram          = SDL_GL_GetProcAddress("glValidateProgram");
    *(void **)&glGenVertexArrays          = SDL_GL_GetProcAddress("glGenVertexArrays");
    *(void **)&glBindVertexArray          = SDL_GL_GetProcAddress("glBindVertexArray");
    *(void **)&glDeleteProgram            = SDL_GL_GetProcAddress("glDeleteProgram");
    *(void **)&glDeleteBuffers            = SDL_GL_GetProcAddress("glDeleteBuffers");
    *(void **)&glDeleteVertexArrays       = SDL_GL_GetProcAddress("glDeleteVertexArrays");
    *(void **)&glUniform1i                = SDL_GL_GetProcAddress("glUniform1i");
    *(void **)&glUniform1f                = SDL_GL_GetProcAddress("glUniform1f");
    *(void **)&glUniform3f                = SDL_GL_GetProcAddress("glUniform3f");
    *(void **)&glGenerateMipmap           = SDL_GL_GetProcAddress("glGenerateMipmap");
    *(void **)&glDeleteShader             = SDL_GL_GetProcAddress("glDeleteShader");
    *(void **)&glGetShaderiv              = SDL_GL_GetProcAddress("glGetShaderiv");
    *(void **)&glGetProgramiv             = SDL_GL_GetProcAddress("glGetProgramiv");
    *(void **)&glGetShaderInfoLog         = SDL_GL_GetProcAddress("glGetShaderInfoLog");

    GL_CHECK(glGenVertexArrays(1, &m_vao));
    GL_CHECK(glBindVertexArray(m_vao));

    onlyOnce = true;
}

void renderer::load(const kdMap &map) {
    // make rendering batches for triangles which share the same texture
    u::vector<uint32_t> indices;
    for (size_t i = 0; i < map.textures.size(); i++) {
        renderTextueBatch batch;
        batch.start = indices.size();
        batch.index = i;
        for (size_t j = 0; j < map.triangles.size(); j++) {
            if (map.triangles[j].texture == i)
                for (size_t k = 0; k < 3; k++)
                    indices.push_back(map.triangles[j].v[k]);
        }
        batch.count = indices.size() - batch.start;
        m_textureBatches.push_back(batch);
    }

    // load textures
    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        m_textureBatches[i].tex.load(map.textures[m_textureBatches[i].index].name);
        //m_textureBatches[i].bump.load("bump_" + u::string(map.textures[m_textureBatches[i].index].name));
        //m_textureBatches[i].tex.bind(GL_TEXTURE0);
        GL_CHECK(m_textureBatches[i].bump.load("nobump.jpg"));
    }

    GL_CHECK(glGenBuffers(2, m_buffers));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &*map.vertices.begin(), GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &*indices.begin(), GL_STATIC_DRAW));

    m_method.enable();
    GL_CHECK(m_method.setTextureUnit(0));
    GL_CHECK(m_method.setNormalUnit(1));

    m_drawElements = indices.size();
}

void renderer::screenShot(const u::string &file) {
    static constexpr size_t kChannels = 3;
    SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, kScreenWidth, kScreenHeight,
        8*kChannels, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

    unsigned char *pixels = new unsigned char[kChannels * kScreenWidth * kScreenHeight];
    switch (kChannels) {
        case 3:
            glReadPixels(0, 0, kScreenWidth, kScreenHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
            break;
        case 4:
            glReadPixels(0, 0, kScreenWidth, kScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            break;
    }

    SDL_LockSurface(temp);
    for (size_t i = 0 ; i < kScreenHeight; i++)
       memcpy(((unsigned char *)temp->pixels) + temp->pitch * i,
            pixels + kChannels * kScreenWidth * (kScreenHeight-i - 1), kScreenWidth * kChannels);
    SDL_UnlockSurface(temp);

    delete[] pixels;

    SDL_SaveBMP(temp, file.c_str());
    SDL_FreeSurface(temp);
}

///! texture
texture::texture(void) :
    m_loaded(false),
    m_textureHandle(0)
{
    //
}

texture::~texture(void) {
    if (m_textureHandle)
        glDeleteTextures(1, &m_textureHandle);
}

void texture::load(const u::string &file) {
    // do not load multiple times
    if (m_loaded)
        return;

    SDL_Surface *surface = IMG_Load(file.c_str());
    if (!surface) {
        fprintf(stderr, "failed to load texture `%s'\n", file.c_str());
        return;
    }

    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    SDL_FreeSurface(surface);
    m_loaded = true;
}

void texture::bind(GLenum unit) {
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);
}

///! method
method::method(void) :
    m_program(0) { }

method::~method(void) {
    for (auto &it : m_shaders)
        glDeleteShader(it);

    if (m_program)
        glDeleteProgram(m_program);
}

bool method::init(void) {
    m_program = glCreateProgram();
    if (!m_program)
        return false;
    return true;
}

bool method::addShader(GLenum shaderType, const char *shaderText) {
    GLuint shaderObject = glCreateShader(shaderType);
    if (!shaderObject)
        return false;

    m_shaders.push_back(shaderObject);

    const GLchar *p[1] = { shaderText };
    const GLint lengths[1] = { (GLint)strlen(shaderText) };
    glShaderSource(shaderObject, 1, p, lengths);
    glCompileShader(shaderObject);

    GLint success = 0;
    glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar log[1024];
        glGetShaderInfoLog(shaderObject, 1024, nullptr, log);
        fprintf(stderr, "%s\n", log);
        return false;
    }

    glAttachShader(m_program, shaderObject);
    return true;
}

void method::enable(void) {
    glUseProgram(m_program);
}

GLint method::getUniformLocation(const char *name) {
    return glGetUniformLocation(m_program, name);
}

bool method::finalize(void) {
    GLint success = 0;
    glLinkProgram(m_program);
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success)
        return false;

    glValidateProgram(m_program);
    glGetProgramiv(m_program, GL_VALIDATE_STATUS, &success);
    if (!success)
        return false;

    for (auto &it : m_shaders)
        glDeleteShader(it);

    m_shaders.clear();
    return true;
}

///! lightMethod
static const char *gLightMethodVertexShader = R"(
    #version 330

    layout (location = 0) in vec3 position;
    layout (location = 1) in vec3 normal;
    layout (location = 2) in vec2 texCoord;
    layout (location = 3) in vec3 tangent;

    uniform mat4 gWVP;
    uniform mat4 gWorld;

    out vec3 normal0;
    out vec2 texCoord0;
    out vec3 tangent0;
    out vec3 worldPos0;

    void main() {
        gl_Position = gWVP * vec4(position, 1.0f);

        texCoord0 = texCoord;
        normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
        worldPos0 = (gWorld * vec4(position, 1.0f)).xyz;
        tangent0 = (gWorld * vec4(tangent, 0.0f)).xyz;
    }
)";

static const char *gLightMethodFragmentShader = R"(
    #version 330

    in vec3 normal0;
    in vec2 texCoord0;
    in vec3 tangent0;
    in vec3 worldPos0;

    struct light {
        vec3 color;
        vec3 direction;
        float ambient;
        float diffuse;
    };

    smooth out vec4 fragColor;

    uniform light gLight;
    uniform sampler2D gSampler;
    uniform sampler2D gNormalMap;
    uniform vec3 gEyeWorldPos;
    uniform float gMatSpecIntensity;
    uniform float gMatSpecPower;

    vec3 calcBump(void) {
        vec3 normal = normalize(normal0);
        vec3 tangent = normalize(tangent0);
        tangent = normalize(tangent - dot(tangent, normal) * normal);
        vec3 bitangent = cross(tangent, normal);
        vec3 bumpMapNormal = texture(gNormalMap, texCoord0).xyz;
        bumpMapNormal = 2.0f * bumpMapNormal - vec3(1.0f, 1.0f, 1.0f);
        vec3 newNormal;
        mat3 tbn = mat3(tangent, bitangent, normal);
        newNormal = tbn * bumpMapNormal;
        newNormal = normalize(newNormal);
        return newNormal;
    }

    void main() {
        vec4 ambientColor = vec4(gLight.color, 1.0f) * gLight.ambient;
        vec3 lightDirection = -gLight.direction;
        vec3 normal = calcBump();

        float diffuseFactor = dot(normal, lightDirection);

        vec4 diffuseColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
        vec4 specularColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);

        if (diffuseFactor > 0.0f) {
            diffuseColor = vec4(gLight.color, 1.0f) * gLight.diffuse * diffuseFactor;

            vec3 vertexToEye = normalize(gEyeWorldPos - worldPos0);
            vec3 lightReflect = normalize(reflect(gLight.direction, normal));
            float specularFactor = dot(vertexToEye, lightReflect);
            specularFactor = pow(specularFactor, gMatSpecPower);
            if (specularFactor > 0.0f)
                specularColor = vec4(gLight.color, 1.0f) * gMatSpecIntensity * specularFactor;
        }

        fragColor = texture2D(gSampler, texCoord0.xy) * (ambientColor + diffuseColor + specularColor);
        fragColor.a = 1.0f;
    }
)";

bool lightMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, gLightMethodVertexShader))
        return false;

    if (!addShader(GL_FRAGMENT_SHADER, gLightMethodFragmentShader))
        return false;

    if (!finalize())
        return false;

    GL_CHECK(m_WVPLocation = getUniformLocation("gWVP"));
    GL_CHECK(m_worldInverse = getUniformLocation("gWorld"));
    GL_CHECK(m_sampler = getUniformLocation("gSampler"));
    GL_CHECK(m_normalMap = getUniformLocation("gNormalMap"));
    GL_CHECK(m_light.color = getUniformLocation("gLight.color"));
    GL_CHECK(m_light.direction = getUniformLocation("gLight.direction"));
    GL_CHECK(m_light.ambient = getUniformLocation("gLight.ambient"));
    GL_CHECK(m_light.diffuse = getUniformLocation("gLight.diffuse"));
    GL_CHECK(m_eyeWorldPosLocation = getUniformLocation("gEyeWorldPos"));
    GL_CHECK(m_matSpecIntensityLocation = getUniformLocation("gMatSpecIntensity"));
    GL_CHECK(m_matSpecPowerLocation = getUniformLocation("gMatSpecPower"));

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void lightMethod::setWorld(const m::mat4 &worldInverse) {
    glUniformMatrix4fv(m_worldInverse, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void lightMethod::setTextureUnit(GLuint unit) {
    glUniform1i(m_sampler, unit);
}

void lightMethod::setNormalUnit(GLuint unit) {
    glUniform1i(m_normalMap, unit);
}

void lightMethod::setLight(const light &l) {
    m::vec3 direction = l.direction;
    direction.normalize();

    GL_CHECK(glUniform3f(m_light.color, l.color.x, l.color.y, l.color.z));
    GL_CHECK(glUniform3f(m_light.direction, direction.x, direction.y, direction.z));
    GL_CHECK(glUniform1f(m_light.ambient, l.ambient));
    GL_CHECK(glUniform1f(m_light.diffuse, l.diffuse));
}

void lightMethod::setEyeWorldPos(const m::vec3 &eyeWorldPos) {
    glUniform3f(m_eyeWorldPosLocation, eyeWorldPos.x, eyeWorldPos.y, eyeWorldPos.z);
}

void lightMethod::setMatSpecIntensity(float intensity) {
    glUniform1f(m_matSpecIntensityLocation, intensity);
}

void lightMethod::setMatSpecPower(float power) {
    glUniform1f(m_matSpecPowerLocation, power);
}
