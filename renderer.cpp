#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "renderer.h"

static constexpr size_t kScreenWidth = 1024;
static constexpr size_t kScreenHeight = 768;

// GL functions have an implicit `this' TLS passed through to them. This
// inherently makes their context global. Thus, we cannot put the context of
// it under a renderer. OpenGL functions themselfs stay global but their handles
// must be obtained via SDL_GL_GetProcAddress.
static bool gOnce = false;

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
    m_scale(1.0f, 1.0f, 1.0f),
    m_worldPosition(0.0f, 0.0f, 0.0f),
    m_rotate(0.0f, 0.0f, 0.0f)
{
    //
}

void rendererPipeline::setScale(float scaleX, float scaleY, float scaleZ) {
    m_scale = m::vec3(scaleX, scaleY, scaleZ);
}

void rendererPipeline::setWorldPosition(float x, float y, float z) {
    m_worldPosition = m::vec3(x, y, z);
}

void rendererPipeline::setRotate(float rotateX, float rotateY, float rotateZ) {
    m_rotate = m::vec3(rotateX, rotateY, rotateZ);
}

void rendererPipeline::setPerspectiveProjection(const perspectiveProjection &projection) {
    m_perspectiveProjection = projection;
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
    translate.setTranslateTrans(m_worldPosition.x, m_worldPosition.y, m_worldPosition.z);

    m_worldTransform = translate * rotate * scale;
    return m_worldTransform;
}

const m::mat4 &rendererPipeline::getWVPTransform(void) {
    getWorldTransform();
    m::mat4 translate, rotate, perspective;
    translate.setTranslateTrans(-m_camera.position.x, -m_camera.position.y, -m_camera.position.z);
    rotate.setCameraTrans(m_camera.target, m_camera.up);
    perspective.setPersProjTrans(
        m_perspectiveProjection.fov,
        m_perspectiveProjection.width,
        m_perspectiveProjection.height,
        m_perspectiveProjection.near,
        m_perspectiveProjection.far);

    m_WVPTransform = perspective * rotate * translate * m_worldTransform;
    return m_WVPTransform;
}

const perspectiveProjection &rendererPipeline::getPerspectiveProjection(void) const {
    return m_perspectiveProjection;
}

const m::vec3 &rendererPipeline::getPosition(void) const {
    return m_camera.position;
}

const m::vec3 &rendererPipeline::getTarget(void) const {
    return m_camera.target;
}

const m::vec3 &rendererPipeline::getUp(void) const {
    return m_camera.up;
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

    fprintf(stderr, ">> loading `%s'\n", file.c_str());
    SDL_Surface *surface = IMG_Load(file.c_str());
    if (!surface)
        return;

    GL_CHECK(glGenTextures(1, &m_textureHandle));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_textureHandle));

    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels));

    GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));

    SDL_FreeSurface(surface);
    m_loaded = true;
}

void texture::bind(GLenum unit) {
    GL_CHECK(glActiveTexture(unit));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_textureHandle));
}

///! textureCubemap
textureCubemap::textureCubemap() :
    m_loaded(false),
    m_textureHandle(0)
{
}

textureCubemap::~textureCubemap(void) {
    if (m_textureHandle)
        glDeleteTextures(1, &m_textureHandle);
}

bool textureCubemap::load(const u::string files[6]) {
    if (m_loaded)
        return false;

    GL_CHECK(glGenTextures(1, &m_textureHandle));
    GL_CHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle));

    GL_CHECK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHECK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));

    for (size_t i = 0; i < 6; i++) {
        fprintf(stderr, ">> loading `%s'\n", files[i].c_str());
        SDL_Surface *surface = IMG_Load(files[i].c_str());
        if (!surface)
            return false;
        GL_CHECK(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, surface->w, surface->h,
            0, GL_RGB, GL_UNSIGNED_BYTE, surface->pixels));
        SDL_FreeSurface(surface);
    }

    m_loaded = true;

    return true;
}

void textureCubemap::bind(GLenum unit) {
    GL_CHECK(glActiveTexture(unit));
    GL_CHECK(glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle));
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

bool method::addShader(GLenum shaderType, const char *shaderFile) {
    FILE *fp = fopen(shaderFile, "r");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *shaderText = new char[length + 1];
    fread(shaderText, length, 1, fp);
    shaderText[length] = '\0';

    fclose(fp);

    GLuint shaderObject = glCreateShader(shaderType);
    if (!shaderObject) {
        delete [] shaderText;
        return false;
    }

    m_shaders.push_back(shaderObject);

    const GLchar *p[1] = { shaderText };
    const GLint lengths[1] = { (GLint)length };
    glShaderSource(shaderObject, 1, p, lengths);
    glCompileShader(shaderObject);
    delete [] shaderText;

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
    GL_CHECK(glUseProgram(m_program));
}

GLint method::getUniformLocation(const char *name) {
    return glGetUniformLocation(m_program, name);
}

GLint method::getUniformLocation(const u::string &name) {
    return glGetUniformLocation(m_program, name.c_str());
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
lightMethod::lightMethod(void)
{

}

bool lightMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/light.vs"))
        return false;

    if (!addShader(GL_FRAGMENT_SHADER, "shaders/light.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldInverse = getUniformLocation("gWorld");
    m_sampler = getUniformLocation("gSampler");
    m_normalMap = getUniformLocation("gNormalMap");

    m_eyeWorldPosLocation = getUniformLocation("gEyeWorldPos");
    m_matSpecIntensityLocation = getUniformLocation("gMatSpecIntensity");
    m_matSpecPowerLocation = getUniformLocation("gMatSpecPower");

    // directional light
    m_directionalLight.color = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLight.ambient = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLight.diffuse = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLight.direction = getUniformLocation("gDirectionalLight.direction");

    // point lights
    m_numPointLights = getUniformLocation("gNumPointLights");
    for (size_t i = 0; i < kMaxPointLights; i++) {
        u::string color = u::format("gPointLights[%zu].base.color", i);
        u::string ambient = u::format("gPointLights[%zu].base.ambient", i);
        u::string diffuse = u::format("gPointLights[%zu].base.diffuse", i);
        u::string constant = u::format("gPointLights[%zu].attenuation.constant", i);
        u::string linear = u::format("gPointLights[%zu].attenuation.linear", i);
        u::string exp = u::format("gPointLights[%zu].attenuation.exp", i);
        u::string position = u::format("gPointLights[%zu].position", i);

        m_pointLights[i].color = getUniformLocation(color);
        m_pointLights[i].ambient = getUniformLocation(ambient);
        m_pointLights[i].diffuse = getUniformLocation(diffuse);
        m_pointLights[i].attenuation.constant = getUniformLocation(constant);
        m_pointLights[i].attenuation.linear = getUniformLocation(linear);
        m_pointLights[i].attenuation.exp = getUniformLocation(exp);
        m_pointLights[i].position = getUniformLocation(position);
    }

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    GL_CHECK(glUniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m));
}

void lightMethod::setWorld(const m::mat4 &worldInverse) {
    GL_CHECK(glUniformMatrix4fv(m_worldInverse, 1, GL_TRUE, (const GLfloat *)worldInverse.m));
}

void lightMethod::setTextureUnit(int unit) {
    GL_CHECK(glUniform1i(m_sampler, unit));
}

void lightMethod::setNormalUnit(int unit) {
    GL_CHECK(glUniform1i(m_normalMap, unit));
}

void lightMethod::setDirectionalLight(const directionalLight &light) {
    m::vec3 direction = light.direction;
    direction.normalize();

    GL_CHECK(glUniform3f(m_directionalLight.color, light.color.x, light.color.y, light.color.z));
    GL_CHECK(glUniform3f(m_directionalLight.direction, direction.x, direction.y, direction.z));
    GL_CHECK(glUniform1f(m_directionalLight.ambient, light.ambient));
    GL_CHECK(glUniform1f(m_directionalLight.diffuse, light.diffuse));
}

void lightMethod::setPointLights(const u::vector<pointLight> &lights) {
    if (lights.size() >= kMaxPointLights) {
        fprintf(stderr, "too many lights");
        abort();
    }

    GL_CHECK(glUniform1i(m_numPointLights, lights.size()));

    for (size_t i = 0; i < lights.size(); i++) {
        GL_CHECK(glUniform3f(m_pointLights[i].color, lights[i].color.x, lights[i].color.y, lights[i].color.z));
        GL_CHECK(glUniform3f(m_pointLights[i].position, lights[i].position.x, lights[i].position.y, lights[i].position.z));
        GL_CHECK(glUniform1f(m_pointLights[i].ambient, lights[i].ambient));
        GL_CHECK(glUniform1f(m_pointLights[i].diffuse, lights[i].diffuse));

        GL_CHECK(glUniform1f(m_pointLights[i].attenuation.constant, lights[i].attenuation.constant));
        GL_CHECK(glUniform1f(m_pointLights[i].attenuation.linear, lights[i].attenuation.linear));
        GL_CHECK(glUniform1f(m_pointLights[i].attenuation.exp, lights[i].attenuation.exp));
    }
}

void lightMethod::setEyeWorldPos(const m::vec3 &eyeWorldPos) {
    GL_CHECK(glUniform3f(m_eyeWorldPosLocation, eyeWorldPos.x, eyeWorldPos.y, eyeWorldPos.z));
}

void lightMethod::setMatSpecIntensity(float intensity) {
    GL_CHECK(glUniform1f(m_matSpecIntensityLocation, intensity));
}

void lightMethod::setMatSpecPower(float power) {
    GL_CHECK(glUniform1f(m_matSpecPowerLocation, power));
}

///! skyboxMethod
bool skyboxMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/skybox.vs"))
        return false;

    if (!addShader(GL_FRAGMENT_SHADER, "shaders/skybox.fs"))
        return false;

    if (!finalize())
        return false;

    GL_CHECK(m_WVPLocation = getUniformLocation("gWVP"));
    GL_CHECK(m_cubeMapLocation = getUniformLocation("gCubemap"));

    return true;
}

void skyboxMethod::setWVP(const m::mat4 &wvp) {
    GL_CHECK(glUniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m));
}

void skyboxMethod::setTextureUnit(int unit) {
    GL_CHECK(glUniform1i(m_cubeMapLocation, unit));
}

///! skybox renderer
skybox::~skybox(void) {
    glDeleteBuffers(1, &m_vbo);
}

bool skybox::init(const u::string &skyboxName) {
    const u::string files[] = {
        skyboxName + "_ft.jpg",
        skyboxName + "_bk.jpg",
        skyboxName + "_up.jpg",
        skyboxName + "_dn.jpg",
        skyboxName + "_rt.jpg",
        skyboxName + "_lf.jpg"
    };

    if (!m_cubemap.load(files)) {
        fprintf(stderr, "couldn't create cubemap\n");
        return false;
    }

    if (!m_method.init()) {
        fprintf(stderr, "failed initializing skybox rendering method\n");
        return false;
    }

    GLfloat vertices[] = {
        -1.0, -1.0,  1.0,
        1.0, -1.0,  1.0,
        -1.0,  1.0,  1.0,
        1.0,  1.0,  1.0,
        -1.0, -1.0, -1.0,
        1.0, -1.0, -1.0,
        -1.0,  1.0, -1.0,
        1.0,  1.0, -1.0,
    };

    GLushort indices[] = { 0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1 };

    GL_CHECK(glGenBuffers(2, m_buffers));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW));

    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

    return true;
}

void skybox::render(const rendererPipeline &pipeline) {
    m_method.enable();

    // to restore later
    GLint faceMode;
    glGetIntegerv(GL_CULL_FACE_MODE, &faceMode);
    GLint depthMode;
    glGetIntegerv(GL_DEPTH_FUNC, &depthMode);

    // get camera position, target and up vectors
    const m::vec3 &position = pipeline.getPosition();
    const m::vec3 &target = pipeline.getTarget();
    const m::vec3 &up = pipeline.getUp();

    // inherit the perspective projection as well
    const perspectiveProjection projection = pipeline.getPerspectiveProjection();

    rendererPipeline p;
    p.setRotate(0.0f, 0.0f, 0.0f);
    p.setWorldPosition(position.x, position.y, position.z);
    p.setCamera(position, target, up);
    p.setPerspectiveProjection(projection);

    m_method.setWVP(p.getWVPTransform());

    // render skybox cube

    glDisable(GL_BLEND);
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture
    m_method.setTextureUnit(0);

    GL_CHECK(glEnableVertexAttribArray(0)); // positions
    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GL_CHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo));
    GL_CHECK(glDrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_SHORT, (void *)0));
    GL_CHECK(glDisableVertexAttribArray(0));

    glCullFace(faceMode);
    glDepthFunc(depthMode);
    glEnable(GL_BLEND);

    glUseProgram(0);
}

///! core renderer
renderer::renderer(void) {
    once();

    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.20f;
    m_directionalLight.diffuse = 0.75f;

    if (!m_method.init()) {
        fprintf(stderr, "failed to initialize rendering method\n");
        abort();
    }

    if (!m_skybox.init("textures/sky")) {
        fprintf(stderr, "failed to load skybox\n");
        abort();
    }
}

renderer::~renderer(void) {
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
}

void renderer::draw(rendererPipeline &p) {
    rendererPipeline copy = p;
    // render the skybox

    const m::mat4 wvp = p.getWVPTransform();
    const m::mat4 worldTransform = p.getWorldTransform();
    const m::vec3 &pos = p.getPosition();

    m_method.enable();
    GL_CHECK(m_method.setTextureUnit(0));
    GL_CHECK(m_method.setNormalUnit(1));

    m_pointLights.clear();
    pointLight pl;
    pl.diffuse = 0.75f;
    pl.ambient = 0.45f;
    pl.color = m::vec3(1.0f, 0.0f, 0.0f);
    pl.position = pos;
    pl.attenuation.linear = 0.1f;

    m_pointLights.push_back(pl);

    // directional light
    m_method.setDirectionalLight(m_directionalLight);

    m_method.setPointLights(m_pointLights);

    m_method.setWVP(wvp);
    m_method.setWorld(worldTransform);

    m_method.setEyeWorldPos(pos);
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

    m_skybox.render(copy);
}

void renderer::once(void) {
    if (gOnce)
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

    // default VAO. We don't utilize it, we just need one for GL 3.3
    GL_CHECK(glGenVertexArrays(1, &m_vao));
    GL_CHECK(glBindVertexArray(m_vao));

    gOnce = true;
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
        GL_CHECK(m_textureBatches[i].bump.load("textures/nobump.jpg"));
    }

    GL_CHECK(glGenBuffers(2, m_buffers));

    GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_vbo));
    GL_CHECK(glBufferData(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &*map.vertices.begin(), GL_STATIC_DRAW));
    GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo));
    GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &*indices.begin(), GL_STATIC_DRAW));
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
