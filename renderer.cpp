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

///! renderer
static const char *gVertexShader = R"(
    #version 330

    layout (location = 0) in vec3 position;
    layout (location = 1) in vec3 normal;
    layout (location = 2) in vec2 texCoord;
    layout (location = 3) in vec3 tangent;

    uniform mat4 gModelViewProjection;
    uniform mat4 gWorld;

    out vec3 normal0;
    out vec2 texCoord0;
    out vec3 tangent0;

    void main() {
        gl_Position = gModelViewProjection * vec4(position, 1.0f);

        normal0 = (gWorld * vec4(normal, 0.0f)).xyz;
        texCoord0 = texCoord;
        tangent0 = tangent;
    }
)";

static const char *gFragmentShader = R"(
    #version 330

    in vec3 normal0;
    in vec2 texCoord0;
    in vec3 tangent0;

    struct directionalLight {
        vec3 color;
        float ambientIntensity;
        float diffuseIntensity;
        vec3 direction;
    };

    smooth out vec4 fragColor;

    uniform directionalLight gDirectionalLight;
    uniform sampler2D gSampler;

    void main() {
        vec4 ambientColor = vec4(gDirectionalLight.color, 1.0f) * gDirectionalLight.ambientIntensity;
        float diffuseFactor = dot(normalize(normal0), -gDirectionalLight.direction);
        vec4 diffuseColor = diffuseFactor > 0.0f
            ? vec4(gDirectionalLight.color, 1.0f) * gDirectionalLight.diffuseIntensity + diffuseFactor
            : vec4(0.0f, 0.0f, 0.0f, 0.0f);

        fragColor = texture2D(gSampler, texCoord0.xy) * (ambientColor + diffuseColor);
        fragColor.a = 1.0f;
    }
)";

static void shaderCompile(GLuint program, const char *text, GLenum type) {
    GLuint obj = glCreateShader(type);
    const GLchar* p[1];
    p[0] = text;
    GLint lengths[1];
    lengths[0] = strlen(text);
    GL_CHECK(glShaderSource(obj, 1, p, lengths));
    GL_CHECK(glCompileShader(obj));
    GL_CHECK(glAttachShader(program, obj));
}

renderer::renderer(void) :
    m_light(
        m::vec3(0.3f, 0.3f, 0.3f),
        0.1f,
        0.70f,
        m::vec3(0.0f, 0.0f, 0.0f)   // direction
    )
{
    once();

    m_program = glCreateProgram();
    shaderCompile(m_program, gVertexShader, GL_VERTEX_SHADER);
    shaderCompile(m_program, gFragmentShader, GL_FRAGMENT_SHADER);
    GL_CHECK(glLinkProgram(m_program));
    GL_CHECK(glValidateProgram(m_program));
    GL_CHECK(glUseProgram(m_program));

    GL_CHECK(m_modelViewProjection = glGetUniformLocation(m_program, "gModelViewProjection"));
    GL_CHECK(m_worldTransform = glGetUniformLocation(m_program, "gWorld"));
    GL_CHECK(m_sampler = glGetUniformLocation(m_program, "gSampler"));

    GL_CHECK(m_light.init(
        glGetUniformLocation(m_program, "gDirectionalLight.color"),
        glGetUniformLocation(m_program, "gDirectionalLight.ambientIntensity"),
        glGetUniformLocation(m_program, "gDirectionalLight.diffuseIntensity"),
        glGetUniformLocation(m_program, "gDirectionalLight.direction")
    ));
}

renderer::~renderer(void) {
    glDeleteProgram(m_program);
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
}

void renderer::draw(rendererPipeline &p) {
    const m::mat4 wvp = p.getWVPTransform();
    const m::mat4 worldTransform = p.getWorldTransform();

    glUniformMatrix4fv(m_modelViewProjection, 1, GL_TRUE, (const GLfloat *)wvp.m);
    glUniformMatrix4fv(m_worldTransform, 1, GL_TRUE, (const GLfloat *)worldTransform.m);

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
    m_texture.bind(GL_TEXTURE0);
    glDrawElements(GL_TRIANGLES, m_drawElements, GL_UNSIGNED_INT, 0);

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

    GL_CHECK(glGenVertexArrays(1, &m_vao));
    GL_CHECK(glBindVertexArray(m_vao));

    onlyOnce = true;
}

void renderer::load(const kdMap &map) {
    // construct all indices, in the future we'll create texture batches for
    // the triangles which share the same textures
    u::vector<uint32_t> indices;
    for (size_t j = 0; j < map.triangles.size(); j++) {
        uint32_t i1 = map.triangles[j].v[0];
        uint32_t i2 = map.triangles[j].v[1];
        uint32_t i3 = map.triangles[j].v[2];
        indices.push_back(i1);
        indices.push_back(i2);
        indices.push_back(i3);
    }

    GL_CHECK(glGenBuffers(2, m_buffers));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &*map.vertices.begin(), GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &*indices.begin(), GL_STATIC_DRAW));

    m_drawElements = indices.size();

    GL_CHECK(glUniform1i(m_sampler, 0));

    m_texture.load("notex.jpg");

    // lighting
    m_light.activate();
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

///! light
light::light(const m::vec3 &color, float ambientIntensity, float diffuseIntensity, const m::vec3 &direction) :
    m_color(color),
    m_ambientIntensity(ambientIntensity),
    m_diffuseIntensity(diffuseIntensity),
    m_direction(direction)
{

}

void light::activate(void) {
    glUniform3f(m_colorLocation, m_color.x, m_color.y, m_color.z);
    glUniform1f(m_ambientIntensityLocation, m_ambientIntensity);
    m::vec3 direction = m_direction.normalized();
    glUniform3f(m_directionLocation, direction.x, direction.y, direction.z);
    glUniform1f(m_diffuseIntensity, m_diffuseIntensity);
}

void light::init(GLuint colorLocation, GLuint ambientIntensityLocation,
    GLuint diffuseIntensityLocation, GLuint directionLocation) {
    m_colorLocation = colorLocation;
    m_ambientIntensityLocation = ambientIntensityLocation;
    m_diffuseIntensityLocation = diffuseIntensityLocation;
    m_directionLocation = directionLocation;
}

///! texture
texture::texture(void) :
    m_textureHandle(0) { }

texture::~texture(void) {
    if (m_textureHandle)
        glDeleteTextures(1, &m_textureHandle);
}

void texture::load(const u::string &file) {
    SDL_Surface *surface = IMG_Load(file.c_str());
    if (!surface)
        return;

    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    SDL_FreeSurface(surface);
}

void texture::bind(GLuint unit) {
    glActiveTexture(unit);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);
}
