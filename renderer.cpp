#include <string.h>

#include <SDL2/SDL.h>

#include "renderer.h"

static PFNGLCREATESHADERPROC             glCreateShader_             = nullptr;
static PFNGLSHADERSOURCEPROC             glShaderSource_             = nullptr;
static PFNGLCOMPILESHADERPROC            glCompileShader_            = nullptr;
static PFNGLATTACHSHADERPROC             glAttachShader_             = nullptr;
static PFNGLCREATEPROGRAMPROC            glCreateProgram_            = nullptr;
static PFNGLLINKPROGRAMPROC              glLinkProgram_              = nullptr;
static PFNGLUSEPROGRAMPROC               glUseProgram_               = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC       glGetUniformLocation_       = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC  glEnableVertexAttribArray_  = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray_ = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC         glUniformMatrix4fv_         = nullptr;
static PFNGLBINDBUFFERPROC               glBindBuffer_               = nullptr;
static PFNGLGENBUFFERSPROC               glGenBuffers_               = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC      glVertexAttribPointer_      = nullptr;
static PFNGLBUFFERDATAPROC               glBufferData_               = nullptr;
static PFNGLVALIDATEPROGRAMPROC          glValidateProgram_          = nullptr;
static PFNGLGENVERTEXARRAYSPROC          glGenVertexArrays_          = nullptr;
static PFNGLBINDVERTEXARRAYPROC          glBindVertexArray_          = nullptr;
static PFNGLDELETEPROGRAMPROC            glDeleteProgram_            = nullptr;
static PFNGLDELETEBUFFERSPROC            glDeleteBuffers_            = nullptr;
static PFNGLDELETEVERTEXARRAYSPROC       glDeleteVertexArrays_       = nullptr;
static PFNGLUNIFORM1IPROC                glUniform1i_                = nullptr;
static PFNGLUNIFORM1FPROC                glUniform1f_                = nullptr;
static PFNGLUNIFORM2FPROC                glUniform2f_                = nullptr;
static PFNGLUNIFORM3FVPROC               glUniform3fv_               = nullptr;
static PFNGLUNIFORM4FPROC                glUniform4f_                = nullptr;
static PFNGLGENERATEMIPMAPPROC           glGenerateMipmap_           = nullptr;
static PFNGLDELETESHADERPROC             glDeleteShader_             = nullptr;
static PFNGLGETSHADERIVPROC              glGetShaderiv_              = nullptr;
static PFNGLGETPROGRAMIVPROC             glGetProgramiv_             = nullptr;
static PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog_         = nullptr;
static PFNGLACTIVETEXTUREPROC            glActiveTexture_            = nullptr;
static PFNGLGENFRAMEBUFFERSPROC          glGenFramebuffers_          = nullptr;
static PFNGLBINDFRAMEBUFFERPROC          glBindFramebuffer_          = nullptr;
static PFNGLFRAMEBUFFERTEXTURE2DPROC     glFramebufferTexture2D_     = nullptr;
static PFNGLDRAWBUFFERSPROC              glDrawBuffers_              = nullptr;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC   glCheckFramebufferStatus_   = nullptr;
static PFNGLDELETEFRAMEBUFFERSPROC       glDeleteFramebuffers_       = nullptr;
static PFNGLBLITFRAMEBUFFERPROC          glBlitFramebuffer_          = nullptr;

///! rendererPipeline
rendererPipeline::rendererPipeline(void) :
    m_scale(1.0f, 1.0f, 1.0f),
    m_worldPosition(0.0f, 0.0f, 0.0f),
    m_rotate(0.0f, 0.0f, 0.0f)
{
    //
}

void rendererPipeline::setScale(const m::vec3 &scale) {
    m_scale = scale;
}
void rendererPipeline::setWorldPosition(const m::vec3 &worldPosition) {
    m_worldPosition = worldPosition;
}
void rendererPipeline::setRotate(const m::vec3 &rotate) {
    m_rotate = rotate;
}
void rendererPipeline::setRotation(const m::quat &rotation) {
    m_rotation = rotation;
}
void rendererPipeline::setPosition(const m::vec3 &position) {
    m_position = position;
}
void rendererPipeline::setPerspectiveProjection(const m::perspectiveProjection &projection) {
    m_perspectiveProjection = projection;
}

const m::mat4 &rendererPipeline::getWorldTransform(void) {
    m::mat4 scale, rotate, translate;
    scale.setScaleTrans(m_scale.x, m_scale.y, m_scale.z);
    rotate.setRotateTrans(m_rotate.x, m_rotate.y, m_rotate.z);
    translate.setTranslateTrans(m_worldPosition.x, m_worldPosition.y, m_worldPosition.z);

    m_worldTransform = translate * rotate * scale;
    return m_worldTransform;
}

const m::mat4 &rendererPipeline::getVPTransform(void) {
    m::mat4 translate, rotate, perspective;
    translate.setTranslateTrans(-m_position.x, -m_position.y, -m_position.z);
    rotate.setCameraTrans(getTarget(), getUp());
    perspective.setPersProjTrans(getPerspectiveProjection());
    m_VPTransform = perspective * rotate * translate;
    return m_VPTransform;
}

const m::mat4 &rendererPipeline::getWVPTransform(void) {
    getWorldTransform();
    getVPTransform();

    m_WVPTransform = m_VPTransform * m_worldTransform;
    return m_WVPTransform;
}

const m::perspectiveProjection &rendererPipeline::getPerspectiveProjection(void) const {
    return m_perspectiveProjection;
}

const m::vec3 rendererPipeline::getTarget(void) const {
    m::vec3 target;
    m_rotation.getOrient(&target, nullptr, nullptr);
    return target;
}
const m::vec3 rendererPipeline::getUp(void) const {
    m::vec3 up;
    m_rotation.getOrient(nullptr, &up, nullptr);
    return up;
}

const m::vec3 &rendererPipeline::getPosition(void) const {
    return m_position;
}
const m::quat &rendererPipeline::getRotation(void) const {
    return m_rotation;
}

///! textures (2D and 3D)
#ifdef GL_UNSIGNED_INT_8_8_8_8_REV
#   define R_TEX_DATA_RGBA GL_UNSIGNED_INT_8_8_8_8_REV
#else
#   define R_TEX_DATA_RGBA GL_UNSINGED_BYTE
#endif
#ifdef GL_UNSIGNED_INT_8_8_8_8
#   define R_TEX_DATA_BGRA GL_UNSIGNED_INT_8_8_8_8
#else
#   define R_TEX_DATA_BGRA GL_UNSIGNED_BYTE
#endif

static void getTextureFormat(const texture &tex, GLenum &tf, GLenum &df) {
    switch (tex.format()) {
        case TEX_RGBA:
            tf = GL_RGBA;
            df = R_TEX_DATA_RGBA;
            break;
        case TEX_BGRA:
            tf = GL_BGRA;
            df = R_TEX_DATA_BGRA;
            break;
        case TEX_RGB:
            tf = GL_RGB;
            df = GL_UNSIGNED_BYTE;
            break;
        case TEX_BGR:
            tf = GL_BGR;
            df = GL_UNSIGNED_BYTE;
            break;
        default:
            // to silent possible uninitialized warnings
            tf = 0;
            df = GL_UNSIGNED_BYTE;
            break;
    }
}

texture2D::texture2D(void) :
    m_uploaded(false),
    m_textureHandle(0)
{
    //
}

texture2D::~texture2D(void) {
    if (m_uploaded)
        glDeleteTextures(1, &m_textureHandle);
}

bool texture2D::load(const u::string &file) {
    return m_texture.load(file);
}

bool texture2D::upload(void) {
    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);

    GLenum tf;
    GLenum df;
    getTextureFormat(m_texture, tf, df);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_texture.width(),
        m_texture.height(), 0, tf, df, m_texture.data());

    glGenerateMipmap_(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    return m_uploaded = true;
}

void texture2D::bind(GLenum unit) {
    glActiveTexture_(unit);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);
}

void texture2D::resize(size_t width, size_t height) {
    m_texture.resize(width, height);
}

///! textureCubemap
texture3D::texture3D() :
    m_uploaded(false),
    m_textureHandle(0)
{
}

texture3D::~texture3D(void) {
    if (m_uploaded)
        glDeleteTextures(1, &m_textureHandle);
}

bool texture3D::load(const u::string &ft, const u::string &bk, const u::string &up,
                     const u::string &dn, const u::string &rt, const u::string &lf)
{
    if (!m_textures[0].load(ft)) return false;
    if (!m_textures[1].load(bk)) return false;
    if (!m_textures[2].load(up)) return false;
    if (!m_textures[3].load(dn)) return false;
    if (!m_textures[4].load(rt)) return false;
    if (!m_textures[5].load(lf)) return false;
    return true;
}

bool texture3D::upload(void) {
    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // find the largest texture in the cubemap and scale all others to it
    size_t mw = 0;
    size_t mh = 0;
    size_t mi = 0;
    for (size_t i = 0; i < 6; i++) {
        if (m_textures[i].width() > mw && m_textures[i].height() > mh)
            mi = i;
    }

    const size_t fw = m_textures[mi].width();
    const size_t fh = m_textures[mi].height();
    for (size_t i = 0; i < 6; i++) {
        if (m_textures[i].width() != fw || m_textures[i].height() != fh)
            m_textures[i].resize(fw, fh);
        GLuint tf, df;
        getTextureFormat(m_textures[i], tf, df);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, fw, fh, 0, tf, df, m_textures[i].data());
    }

    return m_uploaded = true;
}

void texture3D::bind(GLenum unit) {
    glActiveTexture_(unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);
}

void texture3D::resize(size_t width, size_t height) {
    for (size_t i = 0; i < 6; i++)
        m_textures[i].resize(width, height);
}

///! A rendering method
method::method(void) :
    m_program(0),
    m_vertexSource("#version 330 core\n"),
    m_fragmentSource("#version 330 core\n"),
    m_geometrySource("#version 330 core\n")
{

}

method::~method(void) {
    for (auto &it : m_shaders)
        glDeleteShader_(it);

    if (m_program)
        glDeleteProgram_(m_program);
}

bool method::init(void) {
    m_program = glCreateProgram_();
    return !!m_program;
}

bool method::addShader(GLenum shaderType, const char *shaderFile) {
    u::string *shaderSource;
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
    }

    auto source = u::read(shaderFile, "r");
    if (source)
        *shaderSource += u::string((*source).begin(), (*source).end());
    else
        return false;

    GLuint shaderObject = glCreateShader_(shaderType);
    if (!shaderObject)
        return false;

    m_shaders.push_back(shaderObject);

    const GLchar *shaderSources[1];
    GLint shaderLengths[1];

    shaderSources[0] = (GLchar *)&(*shaderSource)[0];
    shaderLengths[0] = (GLint)shaderSource->size();

    glShaderSource_(shaderObject, 1, shaderSources, shaderLengths);
    glCompileShader_(shaderObject);

    GLint shaderCompileStatus = 0;
    glGetShaderiv_(shaderObject, GL_COMPILE_STATUS, &shaderCompileStatus);
    if (shaderCompileStatus == 0) {
        u::string infoLog;
        GLint infoLogLength = 0;
        glGetShaderiv_(shaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog.resize(infoLogLength);
        glGetShaderInfoLog_(shaderObject, infoLogLength, nullptr, &infoLog[0]);
        printf("Shader error:\n%s\n", infoLog.c_str());
        return false;
    }

    glAttachShader_(m_program, shaderObject);
    return true;
}

void method::enable(void) {
    glUseProgram_(m_program);
}

GLint method::getUniformLocation(const char *name) {
    return glGetUniformLocation_(m_program, name);
}

GLint method::getUniformLocation(const u::string &name) {
    return glGetUniformLocation_(m_program, name.c_str());
}

bool method::finalize(void) {
    GLint success = 0;
    glLinkProgram_(m_program);
    glGetProgramiv_(m_program, GL_LINK_STATUS, &success);
    if (!success)
        return false;

    glValidateProgram_(m_program);
    glGetProgramiv_(m_program, GL_VALIDATE_STATUS, &success);
    if (!success)
        return false;

    for (auto &it : m_shaders)
        glDeleteShader_(it);

    m_shaders.clear();
    return true;
}

void method::addVertexPrelude(const u::string &prelude) {
    m_vertexSource += prelude;
    m_vertexSource += "\n";
}

void method::addFragmentPrelude(const u::string &prelude) {
    m_fragmentSource += prelude;
    m_fragmentSource += "\n";
}

void method::addGeometryPrelude(const u::string &prelude) {
    m_geometrySource += prelude;
    m_geometrySource += "\n";
}

gBuffer::gBuffer() :
    m_fbo(0),
    m_depthTexture(0)
{
    memset(m_textures, 0, sizeof(m_textures));
}

gBuffer::~gBuffer() {
    if (m_fbo)
        glDeleteFramebuffers_(1, &m_fbo);

    if (*m_textures)
        glDeleteTextures(kMax, m_textures);

    if (m_depthTexture)
        glDeleteTextures(1, &m_depthTexture);
}

bool gBuffer::init(const m::perspectiveProjection &project) {
    glGenFramebuffers_(1, &m_fbo);
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, m_fbo);

    glGenTextures(kMax, m_textures);
    glGenTextures(1, &m_depthTexture);

    // position (16-bit float)
    glBindTexture(GL_TEXTURE_2D, m_textures[kPosition]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, project.width, project.height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[kPosition], 0);

    // diffuse RGBA
    glBindTexture(GL_TEXTURE_2D, m_textures[kDiffuse]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, project.width, project.height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, m_textures[kDiffuse], 0);

    // normals
    glBindTexture(GL_TEXTURE_2D, m_textures[kNormal]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, project.width, project.height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 2, GL_TEXTURE_2D, m_textures[kNormal], 0);

    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16,
        project.width, project.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glFramebufferTexture2D_(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, m_depthTexture, 0);

    static GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0, // position
        GL_COLOR_ATTACHMENT1, // diffuse
        GL_COLOR_ATTACHMENT2, // normal
        //GL_COLOR_ATTACHMENT3  // texCoord
    };

    glDrawBuffers_(kMax, drawBuffers);

    GLenum status = glCheckFramebufferStatus_(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void gBuffer::bindReading(void) {
    glBindFramebuffer_(GL_READ_FRAMEBUFFER, m_fbo);
}

void gBuffer::bindAccumulate(void) {
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, 0);
    for (size_t i = 0; i < kMax; i++) {
        glActiveTexture_(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, m_textures[kPosition + i]);
    }
}

void gBuffer::bindWriting(void) {
    glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, m_fbo);
}

//// Rendering methods:

///! Light Rendering Method
bool lightMethod::init(const char *vs, const char *fs) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, vs))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, fs))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_positionTextureUnitLocation = getUniformLocation("gPositionMap");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");
    m_normalTextureUnitLocation = getUniformLocation("gNormalMap");
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");
    m_matSpecularIntensityLocation = getUniformLocation("gMatSpecularIntensity");
    m_matSpecularPowerLocation = getUniformLocation("gMatSpecularPower");
    m_screenSizeLocation = getUniformLocation("gScreenSize");

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat*)wvp.m);
}

void lightMethod::setPositionTextureUnit(int unit) {
    glUniform1i_(m_positionTextureUnitLocation, unit);
}

void lightMethod::setColorTextureUnit(int unit) {
    glUniform1i_(m_colorTextureUnitLocation, unit);
}

void lightMethod::setNormalTextureUnit(int unit) {
    glUniform1i_(m_normalTextureUnitLocation, unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    glUniform3fv_(m_eyeWorldPositionLocation, 1, &position.x);
}

void lightMethod::setMatSpecIntensity(float intensity) {
    glUniform1f_(m_matSpecularIntensityLocation, intensity);
}

void lightMethod::setMatSpecPower(float power) {
    glUniform1f_(m_matSpecularPowerLocation, power);
}

void lightMethod::setScreenSize(size_t width, size_t height) {
    glUniform2f_(m_screenSizeLocation, (float)width, (float)height);
}

///! Directional Light Rendering Method
bool directionalLightMethod::init(void) {
    if (!lightMethod::init("shaders/dlight.vs", "shaders/dlight.fs"))
        return false;

    m_directionalLightLocation.color = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLightLocation.ambient = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLightLocation.diffuse = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLightLocation.direction = getUniformLocation("gDirectionalLight.direction");

    return true;
}

void directionalLightMethod::setDirectionalLight(const directionalLight &light) {
    m::vec3 direction = light.direction.normalized();
    glUniform3fv_(m_directionalLightLocation.color, 1, &light.color.x);
    glUniform1f_(m_directionalLightLocation.ambient, light.ambient);
    glUniform3fv_(m_directionalLightLocation.direction, 1, &direction.x);
    glUniform1f_(m_directionalLightLocation.diffuse, light.diffuse);
}

///! Point Light Rendering Method
bool pointLightMethod::init(void) {
    if (!lightMethod::init("shaders/plight.vs", "shaders/plight.fs"))
        return false;

    m_pointLightLocation.color = getUniformLocation("gPointLight.base.color");
    m_pointLightLocation.ambient = getUniformLocation("gPointLight.base.ambient");
    m_pointLightLocation.diffuse = getUniformLocation("gPointLight.base.diffuse");
    m_pointLightLocation.position = getUniformLocation("gPointLight.position");
    m_pointLightLocation.attenuation.constant = getUniformLocation("gPointLight.attenuation.constant");
    m_pointLightLocation.attenuation.linear = getUniformLocation("gPointLight.attenuation.linear");
    m_pointLightLocation.attenuation.exp = getUniformLocation("gPointLight.attenuation.exp");

    return true;
}

void pointLightMethod::setPointLight(const pointLight &light) {
    glUniform3fv_(m_pointLightLocation.color, 1, &light.color.x);
    glUniform3fv_(m_pointLightLocation.position, 1, &light.position.x);
    glUniform1f_(m_pointLightLocation.ambient, light.ambient);
    glUniform1f_(m_pointLightLocation.diffuse, light.diffuse);
    glUniform1f_(m_pointLightLocation.attenuation.constant, light.attenuation.constant);
    glUniform1f_(m_pointLightLocation.attenuation.linear, light.attenuation.linear);
    glUniform1f_(m_pointLightLocation.attenuation.exp, light.attenuation.exp);
}

///! Geomety Rendering Method
bool geomMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/geom.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/geom.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_colorMapLocation = getUniformLocation("gColorMap");

    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    glUniformMatrix4fv_(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void geomMethod::setColorTextureUnit(int unit) {
    glUniform1i_(m_colorMapLocation, unit);
}

///! Skybox Rendering Method
bool skyboxMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/skybox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/skybox.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_cubeMapLocation = getUniformLocation("gCubemap");

    return true;
}

void skyboxMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void skyboxMethod::setTextureUnit(int unit) {
    glUniform1i_(m_cubeMapLocation, unit);
}

void skyboxMethod::setWorld(const m::mat4 &worldInverse) {
    glUniformMatrix4fv_(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

///! SplashScreen Rendering Method
bool splashMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/splash.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/splash.fs"))
        return false;
    if (!finalize())
        return false;

    m_splashTextureLocation = getUniformLocation("gSplashTexture");
    m_resolutionLocation = getUniformLocation("gResolution");
    m_timeLocation = getUniformLocation("gTime");

    return true;
}

void splashMethod::setResolution(const m::perspectiveProjection &project) {
    glUniform2f_(m_resolutionLocation, project.width, project.height);
}

void splashMethod::setTime(float dt) {
    glUniform1f_(m_timeLocation, dt);
}

void splashMethod::setTextureUnit(int unit) {
    glUniform1i_(m_splashTextureLocation, unit);
}

///! Billboard Rendering Method
bool billboardMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/billboard.vs"))
        return false;
    if (!addShader(GL_GEOMETRY_SHADER, "shaders/billboard.gs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/billboard.fs"))
        return false;
    if (!finalize())
        return false;

    m_VPLocation = getUniformLocation("gVP");
    m_cameraPositionLocation = getUniformLocation("gCameraPosition");
    m_colorMapLocation = getUniformLocation("gColorMap");
    m_sizeLocation = getUniformLocation("gSize");

    return true;
}

void billboardMethod::setVP(const m::mat4 &vp) {
    glUniformMatrix4fv_(m_VPLocation, 1, GL_TRUE, (const GLfloat*)vp.m);
}

void billboardMethod::setCamera(const m::vec3 &cameraPosition) {
    glUniform3fv_(m_cameraPositionLocation, 1, &cameraPosition.x);
}

void billboardMethod::setTextureUnit(int unit) {
    glUniform1i_(m_colorMapLocation, unit);
}

void billboardMethod::setSize(float width, float height) {
    glUniform2f_(m_sizeLocation, width, height);
}

///! Depth Pass Method
bool depthMethod::init(void) {
    if (!method::init())
        return false;
    if (!addShader(GL_VERTEX_SHADER, "shaders/depthpass.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/depthpass.fs"))
        return false;
    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    return true;
}

void depthMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

//// Renderers:

///! Skybox Renderer
skybox::~skybox(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
}

bool skybox::load(const u::string &skyboxName) {
    if(!(m_cubemap.load(skyboxName + "_ft.jpg", skyboxName + "_bk.jpg", skyboxName + "_up.jpg",
                        skyboxName + "_dn.jpg", skyboxName + "_rt.jpg", skyboxName + "_lf.jpg")))
    {
        fprintf(stderr, "couldn't load skybox textures\n");
        return false;
    }
    return true;
}

bool skybox::upload(void) {
    if (!m_cubemap.upload()) {
        fprintf(stderr, "failed to upload skybox cubemap\n");
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

    GLubyte indices[] = { 0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1 };

    // create vao to encapsulate state
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    // generate the vbo and ibo
    glGenBuffers_(2, m_buffers);

    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray_(0);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    if (!skyboxMethod::init()) {
        fprintf(stderr, "failed initializing skybox rendering method\n");
        return false;
    }

    return true;
}

void skybox::render(const rendererPipeline &pipeline) {
    enable();

    // to restore later
    GLint faceMode;
    glGetIntegerv(GL_CULL_FACE_MODE, &faceMode);
    GLint depthMode;
    glGetIntegerv(GL_DEPTH_FUNC, &depthMode);

    rendererPipeline worldPipeline = pipeline;

    rendererPipeline p;
    p.setRotate(m::vec3(0.0f, 0.0f, 0.0f));
    p.setWorldPosition(pipeline.getPosition());
    p.setPosition(pipeline.getPosition());
    p.setRotation(pipeline.getRotation());
    p.setPerspectiveProjection(pipeline.getPerspectiveProjection());

    setWVP(p.getWVPTransform());
    setWorld(worldPipeline.getWorldTransform());

    // render skybox cube
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    setTextureUnit(0);
    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture

    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_BYTE, (void *)0);
    glBindVertexArray_(0);

    glCullFace(faceMode);
    glDepthFunc(depthMode);
}

///! Splash Screen Renderer
bool splashScreen::load(const u::string &splashScreen) {
    if (!m_texture.load(splashScreen)) {
        fprintf(stderr, "failed to load splash screen texture\n");
        return false;
    }
    return true;
}

bool splashScreen::upload(void) {
    if (!m_texture.upload()) {
        fprintf(stderr, "failed to upload splash screen texture\n");
        return false;
    }

    if (!m_quad.upload()) {
        fprintf(stderr, "failed to upload quad for splash screen\n");
        return false;
    }

    if (!splashMethod::init()) {
        fprintf(stderr, "failed to initialize splash screen rendering method\n");
        return false;
    }

    return true;
}

void splashScreen::render(float dt, const rendererPipeline &pipeline) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    enable();
    setTextureUnit(0);
    setResolution(pipeline.getPerspectiveProjection());
    setTime(dt);

    m_texture.bind(GL_TEXTURE0);
    m_quad.render();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

///! Billboard Renderer
billboard::billboard() {

}

billboard::~billboard() {
    glDeleteBuffers_(1, &m_vbo);
    glDeleteVertexArrays_(1, &m_vao);
}

bool billboard::load(const u::string &billboardTexture) {
    if (!m_texture.load(billboardTexture))
        return false;
    return true;
}

bool billboard::upload(void) {
    if (!m_texture.upload())
        return false;

    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    glGenBuffers_(1, &m_vbo);
    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, sizeof(m::vec3) * m_positions.size(), &m_positions[0], GL_STATIC_DRAW);

    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray_(0);

    return init();
}

void billboard::render(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    enable();

    setCamera(p.getPosition());
    setVP(p.getVPTransform());
    setSize(16, 16);

    setTextureUnit(0);
    m_texture.bind(GL_TEXTURE0);

    glBindVertexArray_(m_vao);
    glDrawArrays(GL_POINTS, 0, m_positions.size());
    glBindVertexArray_(0);
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

///! Sphere Renderer
sphere::~sphere(void) {
    glDeleteBuffers_(2, m_buffers);
}

bool sphere::load(float radius, size_t rings, size_t sectors) {
    m_sphere.build(radius, rings, sectors);
    return m_sphere.indices.size() != 0;
}

bool sphere::upload(void) {
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    glGenBuffers_(2, m_buffers);
    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, m_sphere.vertices.size() * sizeof(float),
        &m_sphere.vertices[0], GL_STATIC_DRAW);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, m_sphere.indices.size() * sizeof(GLushort),
        &m_sphere.indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray_(0);

    return true;
}

void sphere::render(void) {
    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLES, m_sphere.indices.size(), GL_UNSIGNED_SHORT, 0);
    glBindVertexArray_(0);
}

///! Quad Renderer
quad::~quad(void) {
    glDeleteBuffers_(2, m_buffers);
}

bool quad::upload(void) {
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    glGenBuffers_(2, m_buffers);
    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);

    GLfloat vertices[] = {
        -1.0f,-1.0f, 0.0f, 0.0f,  0.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
         1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
         1.0f,-1.0f, 0.0f, 1.0f,  0.0f,
    };

    GLubyte indices[] = { 0, 1, 2, 0, 2, 3 };

    glBufferData_(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)0); // position
    glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)12); // uvs
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    return true;
}

void quad::render(void) {
    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
    glBindVertexArray_(0);
}

///! World Renderer
world::world(void) {
    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.90f;
    m_directionalLight.diffuse = 0.75f;

    const m::vec3 places[] = {
        m::vec3(153.04, 105.02, 197.67),
        m::vec3(-64.14, 105.02, 328.36),
        m::vec3(-279.83, 105.02, 204.61),
        m::vec3(-458.72, 101.02, 189.58),
        m::vec3(-664.53, 75.02, -1.75),
        m::vec3(-580.69, 68.02, -184.89),
        m::vec3(-104.43, 84.02, -292.99),
        m::vec3(-23.59, 84.02, -292.40),
        m::vec3(333.00, 101.02, 194.46),
        m::vec3(167.13, 101.02, 0.32),
        m::vec3(-63.36, 37.20, 2.30),
        m::vec3(459.97, 68.02, -181.60),
        m::vec3(536.75, 75.01, 2.80),
        m::vec3(-4.61, 117.02, -91.74),
        m::vec3(-2.33, 117.02, 86.34),
        m::vec3(-122.92, 117.02, 84.58),
        m::vec3(-123.44, 117.02, -86.57),
        m::vec3(-300.24, 101.02, -0.15),
        m::vec3(-448.34, 101.02, -156.27),
        m::vec3(-452.94, 101.02, 23.58),
        m::vec3(-206.59, 101.02, -209.52),
        m::vec3(62.59, 101.02, -207.53)
    };

    m_billboards.resize(kBillboardMax);

    pointLight pl;
    pl.diffuse = 0.75f;
    pl.ambient = 0.45f;
    pl.color = m::vec3(1.0f, 0.0f, 0.0f);
    pl.attenuation.linear = 0.1f;

    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        switch (rand() % 4) {
            case 0: m_billboards[kBillboardRail].add(places[i]); break;
            case 1: m_billboards[kBillboardLightning].add(places[i]); break;
            case 2: m_billboards[kBillboardRocket].add(places[i]); break;
            case 3: m_billboards[kBillboardShotgun].add(places[i]); break;
        }
        pl.position = places[i];
        m_pointLights.push_back(pl);
    }
}

world::~world(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
}

bool world::load(const kdMap &map) {
    // load skybox
    if (!m_skybox.load("textures/sky01")) {
        fprintf(stderr, "failed to load skybox\n");
        return false;
    }

    static const struct {
        const char *name;
        const char *file;
        billboardType type;
    } billboards[] = {
        { "railgun",         "textures/railgun.png",   kBillboardRail },
        { "lightning gun",   "textures/lightgun.png",  kBillboardLightning },
        { "rocket launcher", "textures/rocketgun.png", kBillboardRocket },
        { "shotgun",         "textures/shotgun.png",   kBillboardShotgun }
    };

    for (size_t i = 0; i < sizeof(billboards)/sizeof(*billboards); i++) {
        auto &billboard = billboards[i];
        if (m_billboards[billboard.type].load(billboard.file))
            continue;
        fprintf(stderr, "failed to load billboard for `%s'\n", billboard.name);
        return false;
    }

    // make rendering batches for triangles which share the same texture
    for (size_t i = 0; i < map.textures.size(); i++) {
        renderTextueBatch batch;
        batch.start = m_indices.size();
        batch.index = i;
        for (size_t j = 0; j < map.triangles.size(); j++) {
            if (map.triangles[j].texture == i)
                for (size_t k = 0; k < 3; k++)
                    m_indices.push_back(map.triangles[j].v[k]);
        }
        batch.count = m_indices.size() - batch.start;
        m_textureBatches.push_back(batch);
    }

    // load textures
    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        const char *name = map.textures[m_textureBatches[i].index].name;
        const char *find = strrchr(name, '.');

        u::string diffuseName = name;
        u::string normalName = find ? u::string(name, find - name) + "_bump" + find : diffuseName + "_bump";

        texture2D *diffuse = m_textures2D.get(diffuseName);
        texture2D *normal = m_textures2D.get(normalName);
        if (!diffuse)
            diffuse = m_textures2D.get("textures/notex.jpg");
        if (!normal)
            normal = m_textures2D.get("textures/nobump.jpg");

        m_textureBatches[i].diffuse = diffuse;
        m_textureBatches[i].normal = normal;
    }

    m_pointLightSphere.load(1.0f, 24, 24);

    m_vertices = u::move(map.vertices);

    printf("[world] => loaded\n");
    return true;
}

bool world::upload(const m::perspectiveProjection &project) {
    // upload skybox
    if (!m_skybox.upload()) {
        fprintf(stderr, "failed to upload skybox\n");
        return false;
    }

    // upload billboards
    for (auto &it : m_billboards) {
        if (!it.upload()) {
            fprintf(stderr, "failed to upload billboard\n");
            return false;
        }
    }

    // upload textures
    for (auto &it : m_textureBatches) {
        it.diffuse->upload();
        it.normal->upload();
    }

    // upload sphere and quad
    if (!m_pointLightSphere.upload())
        return false;
    if (!m_directionalLightQuad.upload())
        return false;

    // gen vao
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    // gen vbo and ibo
    glGenBuffers_(2, m_buffers);

    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(kdBinVertex), &m_vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    glVertexAttribPointer_(2, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    //glVertexAttribPointer_(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);
    glEnableVertexAttribArray_(2);
    //glEnableVertexAttribArray_(3);

    // upload data
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_STATIC_DRAW);

    if (!m_depthMethod.init()) {
        fprintf(stderr, "failed to initialize depth pass method\n");
        return false;
    }

    if (!m_geomMethod.init()) {
        fprintf(stderr, "failed to initialize geometry rendering method\n");
        return false;
    }

    if (!m_directionalLightMethod.init()) {
        fprintf(stderr, "failed to initialize directional light rendering method\n");
        return false;
    }

    if (!m_pointLightMethod.init()) {
        fprintf(stderr, "failed to initialize point light rendering method\n");
        return false;
    }

    // setup g-buffer
    if (!m_gBuffer.init(project)) {
        fprintf(stderr, "failed to initialize G-buffer\n");
        return false;
    }

    printf("[world] => uploaded\n");
    return true;
}

void world::geometryPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    m_geomMethod.enable();
    m_gBuffer.bindWriting();

    // Only the geometry pass should update the depth buffer
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Only the geometry pass should update the depth buffer
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    m_geomMethod.setWVP(p.getWVPTransform());
    m_geomMethod.setWorld(p.getWorldTransform());

    // Render the world
    m_geomMethod.setColorTextureUnit(0);
    glBindVertexArray_(m_vao);
    for (auto &it : m_textureBatches) {
        it.diffuse->bind(GL_TEXTURE0);
        //it.normal->bind(GL_TEXTURE1);
        glDrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(GLuint) * it.start));
    }
    //glBindVertexArray_(0);

    // The depth buffer is populated and the stencil pass will require it.
    // However, it will not write to it.
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
}

void world::beginLightPass(void) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    m_gBuffer.bindAccumulate();
    glClear(GL_COLOR_BUFFER_BIT);
}

void world::pointLightPass(const rendererPipeline &pipeline) {
    const m::perspectiveProjection &project = pipeline.getPerspectiveProjection();

    m_pointLightMethod.enable();

    m_pointLightMethod.setPositionTextureUnit(gBuffer::kPosition);
    m_pointLightMethod.setColorTextureUnit(gBuffer::kDiffuse);
    m_pointLightMethod.setNormalTextureUnit(gBuffer::kNormal);
    m_pointLightMethod.setScreenSize(project.width, project.height);

    //m_pointLightMethod.setEyeWorldPos(pipeline.getPosition());
    //m_pointLightMethod.setMatSpecIntensity(2.0f);
    //m_pointLightMethod.setMatSpecPower(200.0f);

    rendererPipeline p;
    p.setRotation(pipeline.getRotation());
    p.setPerspectiveProjection(project);

    for (auto &it : m_pointLights) {
        m_pointLightMethod.setPointLight(it);
        p.setWorldPosition(it.position);

        const float sphereScale = pointLight::calcBounding(it);
        const m::vec3 sphere(sphereScale, sphereScale, sphereScale);
        p.setScale(sphere);
        m_pointLightMethod.setWVP(p.getWVPTransform());
        m_pointLightSphere.render();
    }
}

void world::directionalLightPass(const rendererPipeline &pipeline) {
    const m::perspectiveProjection &project = pipeline.getPerspectiveProjection();
    m_directionalLightMethod.enable();

    m_directionalLightMethod.setPositionTextureUnit(gBuffer::kPosition);
    m_directionalLightMethod.setColorTextureUnit(gBuffer::kDiffuse);
    m_directionalLightMethod.setNormalTextureUnit(gBuffer::kNormal);
    m_directionalLightMethod.setDirectionalLight(m_directionalLight);
    m_directionalLightMethod.setScreenSize(project.width, project.height);

    m_directionalLightMethod.setEyeWorldPos(pipeline.getPosition());
    m_directionalLightMethod.setMatSpecIntensity(2.0f);
    m_directionalLightMethod.setMatSpecPower(20.0f);

    m::mat4 wvp;
    wvp.loadIdentity();

    m_directionalLightMethod.setWVP(wvp);
    m_directionalLightQuad.render();
}

void world::depthPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;
    m_depthMethod.enable();
    m_depthMethod.setWVP(p.getWVPTransform());

    glEnable(GL_DEPTH_TEST); // make sure depth testing is enabled
    glClear(GL_DEPTH_BUFFER_BIT); // clear

    // do a depth pass
    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
}

void world::depthPrePass(const rendererPipeline &pipeline) {
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glColorMask(0.0f, 0.0f, 0.0f, 0.0f);

    depthPass(pipeline);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glColorMask(1.0f, 1.0f, 1.0f, 1.0f);
}

void world::render(const rendererPipeline &pipeline) {
    // depth pre pass
    depthPrePass(pipeline);

    // geometry pass
    geometryPass(pipeline);

    // begin lighting pass
    beginLightPass();

    // point light pass
    pointLightPass(pipeline);

    // directional light pass
    directionalLightPass(pipeline);

    //glBindFramebuffer_(GL_DRAW_FRAMEBUFFER, 0);

    // deferred rendering is done, reestablish depth buffer for forward
    // rendering.
    //depthPass(pipeline);

    // Now standard forward rendering
    m_gBuffer.bindReading();
    glEnable(GL_DEPTH_TEST);
    m_skybox.render(pipeline);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto &it : m_billboards)
        it.render(pipeline);
}

//// Miscellaneous

void initGL(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // back face culling
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    // depth buffer + depth test
    //glClearDepth(1.0f);
    //glDepthFunc(GL_LEQUAL);
    //glEnable(GL_DEPTH_TEST);

    // shade model
    glShadeModel(GL_SMOOTH);
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // multisample anti-aliasing
    //glEnable(GL_MULTISAMPLE);

    #define GL_RESOLVE(NAME) \
        do { \
            union { \
                void *p; \
                decltype(NAME##_) gl; \
            } cast = { SDL_GL_GetProcAddress(#NAME) }; \
            if (!(NAME##_ = cast.gl)) \
                abort(); \
        } while (0)

    GL_RESOLVE(glCreateShader);
    GL_RESOLVE(glShaderSource);
    GL_RESOLVE(glCompileShader);
    GL_RESOLVE(glAttachShader);
    GL_RESOLVE(glCreateProgram);
    GL_RESOLVE(glLinkProgram);
    GL_RESOLVE(glUseProgram);
    GL_RESOLVE(glGetUniformLocation);
    GL_RESOLVE(glEnableVertexAttribArray);
    GL_RESOLVE(glDisableVertexAttribArray);
    GL_RESOLVE(glUniformMatrix4fv);
    GL_RESOLVE(glBindBuffer);
    GL_RESOLVE(glGenBuffers);
    GL_RESOLVE(glVertexAttribPointer);
    GL_RESOLVE(glBufferData);
    GL_RESOLVE(glValidateProgram);
    GL_RESOLVE(glGenVertexArrays);
    GL_RESOLVE(glBindVertexArray);
    GL_RESOLVE(glDeleteProgram);
    GL_RESOLVE(glDeleteBuffers);
    GL_RESOLVE(glDeleteVertexArrays);
    GL_RESOLVE(glUniform1i);
    GL_RESOLVE(glUniform1f);
    GL_RESOLVE(glUniform2f);
    GL_RESOLVE(glUniform3fv);
    GL_RESOLVE(glUniform4f);
    GL_RESOLVE(glGenerateMipmap);
    GL_RESOLVE(glDeleteShader);
    GL_RESOLVE(glGetShaderiv);
    GL_RESOLVE(glGetProgramiv);
    GL_RESOLVE(glGetShaderInfoLog);
    GL_RESOLVE(glActiveTexture);
    GL_RESOLVE(glGenFramebuffers);
    GL_RESOLVE(glBindFramebuffer);
    GL_RESOLVE(glFramebufferTexture2D);
    GL_RESOLVE(glDrawBuffers);
    GL_RESOLVE(glCheckFramebufferStatus);
    GL_RESOLVE(glDeleteFramebuffers);
    GL_RESOLVE(glBlitFramebuffer);
}

void screenShot(const u::string &file, const m::perspectiveProjection &project) {
    const size_t screenWidth = project.width;
    const size_t screenHeight = project.height;
    const size_t screenSize = screenWidth * screenHeight;
    SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, screenWidth, screenHeight,
        8*3, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_LockSurface(temp);
    unsigned char *pixels = new unsigned char[screenSize * 3];
    // make sure we're reading from the final framebuffer when obtaining the pixels
    // for the screenshot.
    glBindFramebuffer_(GL_READ_FRAMEBUFFER, 0);
    glReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    texture::reorient(pixels, screenWidth, screenHeight, 3, screenWidth * 3,
        (unsigned char *)temp->pixels, false, true, false);
    SDL_UnlockSurface(temp);
    delete[] pixels;
    SDL_SaveBMP(temp, file.c_str());
    SDL_FreeSurface(temp);
}
