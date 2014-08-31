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
    size_t preludeLength = 0;
    u::string *shaderSource;
    switch (shaderType) {
        case GL_VERTEX_SHADER:
            preludeLength = m_vertexSource.size();
            shaderSource = &m_vertexSource;
            break;
        case GL_FRAGMENT_SHADER:
            preludeLength = m_fragmentSource.size();
            shaderSource = &m_fragmentSource;
            break;
        case GL_GEOMETRY_SHADER:
            preludeLength = m_geometrySource.size();
            shaderSource = &m_geometrySource;
            break;
    }

    FILE *fp = fopen(shaderFile, "r");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size_t shaderLength = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    shaderSource->resize(preludeLength + shaderLength);
    fread(&(*shaderSource)[preludeLength], shaderLength, 1, fp);
    fclose(fp);

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

//// Rendering methods:

///! Light Rendering Method
lightMethod::lightMethod(void)
{

}

bool lightMethod::init(void) {
    if (!method::init())
        return false;

    addFragmentPrelude(u::format("const int kMaxPointLights = %zu;", kMaxPointLights));
    addFragmentPrelude(u::format("const int kMaxSpotLights = %zu;", kMaxSpotLights));

    if (!addShader(GL_VERTEX_SHADER, "shaders/light.vs"))
        return false;

    if (!addShader(GL_FRAGMENT_SHADER, "shaders/light.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_samplerLocation = getUniformLocation("gSampler");
    m_normalMapLocation = getUniformLocation("gNormalMap");

    m_eyeWorldPosLocation = getUniformLocation("gEyeWorldPos");
    m_matSpecIntensityLocation = getUniformLocation("gMatSpecIntensity");
    m_matSpecPowerLocation = getUniformLocation("gMatSpecPower");

    // directional light
    m_directionalLight.colorLocation = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLight.ambientLocation = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLight.diffuseLocation = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLight.directionLocation = getUniformLocation("gDirectionalLight.direction");

    // point lights
    m_numPointLightsLocation = getUniformLocation("gNumPointLights");
    for (size_t i = 0; i < kMaxPointLights; i++) {
        u::string color = u::format("gPointLights[%zu].base.color", i);
        u::string ambient = u::format("gPointLights[%zu].base.ambient", i);
        u::string diffuse = u::format("gPointLights[%zu].base.diffuse", i);
        u::string constant = u::format("gPointLights[%zu].attenuation.constant", i);
        u::string linear = u::format("gPointLights[%zu].attenuation.linear", i);
        u::string exp = u::format("gPointLights[%zu].attenuation.exp", i);
        u::string position = u::format("gPointLights[%zu].position", i);

        m_pointLights[i].colorLocation = getUniformLocation(color);
        m_pointLights[i].ambientLocation = getUniformLocation(ambient);
        m_pointLights[i].diffuseLocation = getUniformLocation(diffuse);
        m_pointLights[i].attenuation.constantLocation = getUniformLocation(constant);
        m_pointLights[i].attenuation.linearLocation = getUniformLocation(linear);
        m_pointLights[i].attenuation.expLocation = getUniformLocation(exp);
        m_pointLights[i].positionLocation = getUniformLocation(position);
    }

    // spot lights
    m_numSpotLightsLocation = getUniformLocation("gNumSpotLights");
    for (size_t i = 0; i < kMaxSpotLights; i++) {
        u::string color = u::format("gSpotLights[%zu].base.base.color", i);
        u::string ambient = u::format("gSpotLights[%zu].base.base.ambient", i);
        u::string diffuse = u::format("gSpotLights[%zu].base.base.diffuse", i);
        u::string constant = u::format("gSpotLights[%zu].base.attenuation.constant", i);
        u::string linear = u::format("gSpotLights[%zu].base.attenuation.linear", i);
        u::string exp = u::format("gSpotLights[%zu].base.attenuation.exp", i);
        u::string position = u::format("gSpotLights[%zu].base.position", i);
        u::string direction = u::format("gSpotLights[%zu].direction", i);
        u::string cutOff = u::format("gSpotLights[%zu].cutOff", i);

        m_spotLights[i].colorLocation = getUniformLocation(color);
        m_spotLights[i].ambientLocation = getUniformLocation(ambient);
        m_spotLights[i].diffuseLocation = getUniformLocation(diffuse);
        m_spotLights[i].attenuation.constantLocation = getUniformLocation(constant);
        m_spotLights[i].attenuation.linearLocation = getUniformLocation(linear);
        m_spotLights[i].attenuation.expLocation = getUniformLocation(exp);
        m_spotLights[i].positionLocation = getUniformLocation(position);
        m_spotLights[i].directionLocation = getUniformLocation(direction);
        m_spotLights[i].cutOffLocation = getUniformLocation(cutOff);
    }

    // fog
    m_fog.colorLocation = getUniformLocation("gFog.color");
    m_fog.densityLocation = getUniformLocation("gFog.density");
    m_fog.endLocation = getUniformLocation("gFog.end");
    m_fog.methodLocation = getUniformLocation("gFog.method");
    m_fog.startLocation = getUniformLocation("gFog.start");

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void lightMethod::setWorld(const m::mat4 &worldInverse) {
    glUniformMatrix4fv_(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void lightMethod::setTextureUnit(int unit) {
    glUniform1i_(m_samplerLocation, unit);
}

void lightMethod::setNormalUnit(int unit) {
    glUniform1i_(m_normalMapLocation, unit);
}

void lightMethod::setDirectionalLight(const directionalLight &light) {
    m::vec3 direction = light.direction;
    direction.normalize();

    glUniform3fv_(m_directionalLight.colorLocation, 1, &light.color.x);
    glUniform3fv_(m_directionalLight.directionLocation, 1, &direction.x);
    glUniform1f_(m_directionalLight.ambientLocation, light.ambient);
    glUniform1f_(m_directionalLight.diffuseLocation, light.diffuse);
}

void lightMethod::setPointLights(u::vector<pointLight> &lights) {
    // limit the number of maximum lights
    if (lights.size() > kMaxPointLights)
        lights.resize(kMaxPointLights);

    glUniform1i_(m_numPointLightsLocation, lights.size());

    for (size_t i = 0; i < lights.size(); i++) {
        glUniform3fv_(m_pointLights[i].colorLocation, 1, &lights[i].color.x);
        glUniform3fv_(m_pointLights[i].positionLocation, 1, &lights[i].position.x);
        glUniform1f_(m_pointLights[i].ambientLocation, lights[i].ambient);
        glUniform1f_(m_pointLights[i].diffuseLocation, lights[i].diffuse);
        glUniform1f_(m_pointLights[i].attenuation.constantLocation, lights[i].attenuation.constant);
        glUniform1f_(m_pointLights[i].attenuation.linearLocation, lights[i].attenuation.linear);
        glUniform1f_(m_pointLights[i].attenuation.expLocation, lights[i].attenuation.exp);
    }
}

void lightMethod::setSpotLights(u::vector<spotLight> &lights) {
    if (lights.size() > kMaxSpotLights)
        lights.resize(kMaxSpotLights);

    glUniform1i_(m_numSpotLightsLocation, lights.size());

    for (size_t i = 0; i < lights.size(); i++) {
        m::vec3 direction = lights[i].direction.normalized();
        glUniform3fv_(m_spotLights[i].colorLocation, 1, &lights[i].color.x);
        glUniform3fv_(m_spotLights[i].positionLocation, 1, &lights[i].position.x);
        glUniform3fv_(m_spotLights[i].directionLocation, 1, &direction.x);
        glUniform1f_(m_spotLights[i].cutOffLocation, cosf(m::toRadian(lights[i].cutOff)));
        glUniform1f_(m_spotLights[i].ambientLocation, lights[i].ambient);
        glUniform1f_(m_spotLights[i].diffuseLocation, lights[i].diffuse);
        glUniform1f_(m_spotLights[i].attenuation.constantLocation, lights[i].attenuation.constant);
        glUniform1f_(m_spotLights[i].attenuation.linearLocation, lights[i].attenuation.linear);
        glUniform1f_(m_spotLights[i].attenuation.expLocation, lights[i].attenuation.exp);
    }
}

void lightMethod::setEyeWorldPos(const m::vec3 &eyeWorldPos) {
    glUniform3fv_(m_eyeWorldPosLocation, 1, &eyeWorldPos.x);
}

void lightMethod::setMatSpecIntensity(float intensity) {
    glUniform1f_(m_matSpecIntensityLocation, intensity);
}

void lightMethod::setMatSpecPower(float power) {
    glUniform1f_(m_matSpecPowerLocation, power);
}

void lightMethod::setFogType(fogType type) {
    glUniform1i_(m_fog.methodLocation, type);
}

void lightMethod::setFogDistance(float start, float end) {
    glUniform1f_(m_fog.startLocation, start);
    glUniform1f_(m_fog.endLocation, end);
}

void lightMethod::setFogDensity(float density) {
    glUniform1f_(m_fog.densityLocation, density);
}

void lightMethod::setFogColor(const m::vec3 &color) {
    glUniform4f_(m_fog.colorLocation, color.x, color.y, color.z, 1.0f);
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

///! Depth Pre-Pass Rendering Method
bool depthPrePassMethod::init(void) {
    if (!method::init())
        return false;

    if (!addShader(GL_VERTEX_SHADER, "shaders/zprepass.vs"))
        return false;

    if (!addShader(GL_FRAGMENT_SHADER, "shaders/zprepass.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    return true;
}

void depthPrePassMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv_(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
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
splashScreen::~splashScreen(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
}

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

    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    glGenBuffers_(2, m_buffers);
    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);

    GLfloat vertices[] = {
         1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
         1.0f,-1.0f, 0.0f, 1.0f,  0.0f,
        -1.0f,-1.0f, 0.0f, 0.0f,  0.0f,
    };

    GLubyte indices[] = { 0, 1, 2, 2, 1, 3 };

    glBufferData_(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)0); // position
    glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)12); // uvs
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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

    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    glUseProgram_(0);
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

    glDepthMask(GL_TRUE);
    glBindVertexArray_(m_vao);

    glDrawArrays(GL_POINTS, 0, m_positions.size());

    glBindVertexArray_(0);
    glDepthMask(GL_FALSE);
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

///! World Renderer
world::world(void) {
    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.90f;
    m_directionalLight.diffuse = 0.75f;

    billboard rail;
    billboard lightning;
    billboard rocket;
    billboard shotgun;

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

    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        switch (rand() % 4) {
            case 0: rail.add(places[i]); break;
            case 1: lightning.add(places[i]); break;
            case 2: rocket.add(places[i]); break;
            case 3: shotgun.add(places[i]); break;
        }
    }

    m_billboards.resize(kBillboardMax);
    m_billboards[kBillboardRail] = rail;
    m_billboards[kBillboardLightning] = lightning;
    m_billboards[kBillboardRocket] = rocket;
    m_billboards[kBillboardShotgun] = shotgun;
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
    return true;
}

bool world::upload(const kdMap &map) {
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

    // gen vao
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    // gen vbo and ibo
    glGenBuffers_(2, m_buffers);

    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &map.vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    glVertexAttribPointer_(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    glVertexAttribPointer_(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    glVertexAttribPointer_(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);
    glEnableVertexAttribArray_(2);
    glEnableVertexAttribArray_(3);

    // upload data
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t), &m_indices[0], GL_STATIC_DRAW);

    if (!m_depthPrePassMethod.init()) {
        fprintf(stderr, "failed to initialize depth prepass rendering method\n");
        return false;
    }

    if (!m_lightMethod.init()) {
        fprintf(stderr, "failed to initialize world lighting rendering method\n");
        return false;
    }

    return true;
}

void world::draw(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    // Z prepass
    glDepthFunc(GL_LESS);
    glColorMask(0.0f, 0.0f, 0.0f, 0.0f);
    glDepthMask(GL_TRUE);

    m_depthPrePassMethod.enable();
    m_depthPrePassMethod.setWVP(p.getWVPTransform());

    // render geometry only
    glBindVertexArray_(m_vao);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, (const GLvoid*)0);
    glBindVertexArray_(0);

    // normal pass
    glDepthFunc(GL_LEQUAL);
    glColorMask(1.0f, 1.0f, 1.0f, 1.0f);
    glDepthMask(GL_FALSE);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    render(pipeline);

    // needs to be enabled to clear the depth buffer bit
    glDepthMask(GL_TRUE);
}

void world::render(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    m_lightMethod.enable();
    m_lightMethod.setTextureUnit(0);
    m_lightMethod.setNormalUnit(1);

    static bool setFog = true;
    if (setFog) {
        m_lightMethod.setFogColor(m::vec3(0.8f, 0.8f, 0.8f));
        m_lightMethod.setFogDensity(0.0007f);
        m_lightMethod.setFogType(kFogExp);
        setFog = false;
    }

    //setFogDistance(110.0f, 470.0f);
    //setFogType(kFogLinear);

    m_lightMethod.setDirectionalLight(m_directionalLight);

    m_lightMethod.setPointLights(m_pointLights);
    m_lightMethod.setSpotLights(m_spotLights);

    m_lightMethod.setWVP(p.getWVPTransform());
    m_lightMethod.setWorld(p.getWorldTransform());

    m_lightMethod.setEyeWorldPos(p.getPosition());
    m_lightMethod.setMatSpecIntensity(2.0f);
    m_lightMethod.setMatSpecPower(200.0f);

    glBindVertexArray_(m_vao);
    for (auto &it : m_textureBatches) {
        it.diffuse->bind(GL_TEXTURE0);
        it.normal->bind(GL_TEXTURE1);
        glDrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(uint32_t) * it.start));
    }
    glBindVertexArray_(0);

    m_skybox.render(p);

    // render billboards
    for (auto &it : m_billboards)
        it.render(p);
}

//// Miscellaneous

void initGL(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // back face culling
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);

    // depth buffer + depth test
    glClearDepth(1.0f);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);

    // shade model
    glShadeModel(GL_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // multisample anti-aliasing
    glEnable(GL_MULTISAMPLE);

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
}

void screenShot(const u::string &file, const m::perspectiveProjection &project) {
    const size_t screenWidth = project.width;
    const size_t screenHeight = project.height;
    const size_t screenSize = screenWidth * screenHeight;
    SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, screenWidth, screenHeight,
        8*3, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_LockSurface(temp);
    unsigned char *pixels = new unsigned char[screenSize * 3];
    glReadPixels(0, 0, screenWidth, screenHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    texture::reorient(pixels, screenWidth, screenHeight, 3, screenWidth * 3,
        (unsigned char *)temp->pixels, false, true, false);
    SDL_UnlockSurface(temp);
    delete[] pixels;
    SDL_SaveBMP(temp, file.c_str());
    SDL_FreeSurface(temp);
}
