#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "renderer.h"

static constexpr size_t kScreenWidth = 1600;
static constexpr size_t kScreenHeight = 852;

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

///! method
method::method(void) :
    m_program(0),
    m_vertexSource("#version 330 core\n"),
    m_fragmentSource("#version 330 core\n")
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
    }

    FILE *fp = fopen(shaderFile, "r");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size_t shaderLength = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    shaderSource->resize(preludeLength + shaderLength);
    fread((&*shaderSource->begin()) + preludeLength, shaderLength, 1, fp);
    fclose(fp);

    GLuint shaderObject = glCreateShader_(shaderType);
    if (!shaderObject)
        return false;

    m_shaders.push_back(shaderObject);

    const GLchar *shaderSources[1];
    GLint shaderLengths[1];

    shaderSources[0] = (GLchar *)&*shaderSource->begin();
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
        glGetShaderInfoLog_(shaderObject, infoLogLength, nullptr, &*infoLog.begin());
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

///! lightMethod
lightMethod::lightMethod(void)
{

}

bool lightMethod::init(void) {
    if (!method::init())
        return false;

    addFragmentPrelude(u::format("const int kMaxPointLights = %zu;", kMaxPointLights));

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

///! skybox renderer
skybox::~skybox(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
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

    if (!skyboxMethod::init()) {
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

    return true;
}

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
    m_aspectRatioLocation = getUniformLocation("gAspectRatio");

    return true;
}

void splashMethod::setAspectRatio(void) {
    glUniform1f_(m_aspectRatioLocation, (float)kScreenWidth / (float)kScreenHeight);
}

void splashMethod::setTextureUnit(int unit) {
    glUniform1i_(m_splashTextureLocation, unit);
}

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

void skybox::render(const rendererPipeline &pipeline) {
    enable();

    // to restore later
    GLint faceMode;
    glGetIntegerv(GL_CULL_FACE_MODE, &faceMode);
    GLint depthMode;
    glGetIntegerv(GL_DEPTH_FUNC, &depthMode);

    rendererPipeline worldPipeline = pipeline;

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

    setWVP(p.getWVPTransform());
    setWorld(worldPipeline.getWorldTransform());

    // render skybox cube

    glDisable(GL_BLEND);
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture
    setTextureUnit(0);

    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_SHORT, (void *)0);

    glCullFace(faceMode);
    glDepthFunc(depthMode);
    glEnable(GL_BLEND);

    glUseProgram_(0);
}

///! splash screen renderer
splashScreen::~splashScreen(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
}

bool splashScreen::init(const u::string &file) {
    if (!m_texture.load(file)) {
        fprintf(stderr, "failed to load texture `%s'\n", file.c_str());
        return false;
    }

    if (!splashMethod::init()) {
        fprintf(stderr, "failed to initialize splash screen rendering method\n");
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

    GLushort indices[] = { 0, 1, 2, 2, 1, 3 };

    glBufferData_(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)0); // position
    glVertexAttribPointer_(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)12); // uvs
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    return true;
}

void splashScreen::render(void) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    enable();
    setTextureUnit(0);
    setAspectRatio();

    m_texture.bind(GL_TEXTURE0);

    glBindVertexArray_(m_vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

///! world renderer
world::world(void) {
    m_directionalLight.color = m::vec3(0.8f, 0.8f, 0.8f);
    m_directionalLight.direction = m::vec3(-1.0f, 1.0f, 0.0f);
    m_directionalLight.ambient = 0.90f;
    m_directionalLight.diffuse = 0.75f;

    if (!m_depthPrePassMethod.init()) {
        fprintf(stderr, "failed to initialize depth prepass rendering method\n");
        abort();
    }

    if (!m_lightMethod.init()) {
        fprintf(stderr, "failed to initialize world lighting rendering method\n");
        abort();
    }

    if (!m_skybox.init("textures/sky01")) {
        fprintf(stderr, "failed to load skybox\n");
        abort();
    }
}

world::~world(void) {
    glDeleteBuffers_(2, m_buffers);
    glDeleteVertexArrays_(1, &m_vao);
}

bool world::load(const kdMap &map) {
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
        const char *name = map.textures[m_textureBatches[i].index].name;
        const char *find = strrchr(name, '.');

        u::string diffuseName = name;
        u::string normalName = find ? u::string(name, find - name) + "_bump" + find : diffuseName + "_bump";

        texture *diffuse = m_textureManager.get(diffuseName);
        texture *normal = m_textureManager.get(normalName);
        if (!diffuse)
            diffuse = m_textureManager.get("textures/notex.jpg");
        if (!normal)
            normal = m_textureManager.get("textures/nobump.jpg");

        m_textureBatches[i].diffuse = diffuse;
        m_textureBatches[i].normal = normal;
    }

    // gen vao
    glGenVertexArrays_(1, &m_vao);
    glBindVertexArray_(m_vao);

    // gen vbo and ibo
    glGenBuffers_(2, m_buffers);

    glBindBuffer_(GL_ARRAY_BUFFER, m_vbo);
    glBufferData_(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &*map.vertices.begin(), GL_STATIC_DRAW);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    glVertexAttribPointer_(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    glVertexAttribPointer_(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    glVertexAttribPointer_(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    glEnableVertexAttribArray_(0);
    glEnableVertexAttribArray_(1);
    glEnableVertexAttribArray_(2);
    glEnableVertexAttribArray_(3);

    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &*indices.begin(), GL_STATIC_DRAW);

    printf("textures:\n");
    printf("loaded: %zu\n", m_textureManager.loaded());
    printf("shared: %zu\n", m_textureManager.reuses());

    return true;
}

void world::draw(const rendererPipeline &pipeline) {
    rendererPipeline depthPrePassPipeline = pipeline;

    // Z prepass
    glColorMask(0.0f, 0.0f, 0.0f, 0.0f);
    glDepthFunc(GL_LESS);

    m_depthPrePassMethod.enable();
    m_depthPrePassMethod.setWVP(depthPrePassPipeline.getWVPTransform());

    // render geometry only
    glBindVertexArray_(m_vao);
    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        glDrawElements(GL_TRIANGLES, m_textureBatches[i].count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(uint32_t) * m_textureBatches[i].start));
    }
    glBindVertexArray_(0);

    // normal pass
    glColorMask(1.0f, 1.0f, 1.0f, 1.0f);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    render(pipeline);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glDepthMask(GL_TRUE);
}

void world::render(const rendererPipeline &pipeline) {
    rendererPipeline worldPipeline = pipeline;
    rendererPipeline skyPipeline = pipeline;

    const m::mat4 wvp = worldPipeline.getWVPTransform();
    const m::mat4 worldTransform = worldPipeline.getWorldTransform();
    const m::vec3 &pos = worldPipeline.getPosition();

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

    m_pointLights.clear();
    pointLight pl;
    pl.diffuse = 0.75f;
    pl.ambient = 0.45f;
    pl.color = m::vec3(1.0f, 0.0f, 0.0f);
    pl.position = pos;
    pl.attenuation.linear = 0.1f;

    m_pointLights.push_back(pl);

    m_lightMethod.setDirectionalLight(m_directionalLight);

    m_lightMethod.setPointLights(m_pointLights);

    m_lightMethod.setWVP(wvp);
    m_lightMethod.setWorld(worldTransform);

    m_lightMethod.setEyeWorldPos(pos);
    m_lightMethod.setMatSpecIntensity(2.0f);
    m_lightMethod.setMatSpecPower(200.0f);

    glBindVertexArray_(m_vao);
    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        m_textureBatches[i].diffuse->bind(GL_TEXTURE0);
        m_textureBatches[i].normal->bind(GL_TEXTURE1);
        glDrawElements(GL_TRIANGLES, m_textureBatches[i].count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(uint32_t) * m_textureBatches[i].start));
    }
    glBindVertexArray_(0);

    m_skybox.render(skyPipeline);
}

//! textures
template <size_t S>
static void textureHalve(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst) {
    for (unsigned char *yend = src + sh * stride; src < yend;) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += 2 * S, dst += S) {
            for (size_t i = 0; i < S; i++)
                dst[i] = (size_t(xsrc[i]) + size_t(xsrc[i+S]) + size_t(xsrc[stride+i]) + size_t(xsrc[stride+i+S])) >> 2;
        }
        src += 2 * stride;
    }
}

template <size_t S>
static void textureShift(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = sw/dw, hfrac = sh/dh, wshift = 0, hshift = 0;
    while (dw << wshift < sw) wshift++;
    while (dh << hshift < sh) hshift++;
    size_t tshift = wshift + hshift;
    for (unsigned char *yend = src + sh * stride; src < yend; ) {
        for (unsigned char *xend = src + sw * S, *xsrc = src; xsrc < xend; xsrc += wfrac * S, dst += S) {
            size_t r[S] = {0};
            for (unsigned char *ycur = xsrc, *xend = ycur + wfrac * S,
                    *yend = src + hfrac * stride; ycur<yend; ycur += stride, xend += stride) {
                for (unsigned char *xcur = ycur; xcur < xend; xcur += S) {
                    for (size_t i = 0; i < S; i++)
                        r[i] += xcur[i];
                }
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i]) >> tshift;
        }
        src += hfrac*stride;
    }
}

template <size_t S>
static void textureScale(unsigned char *src, size_t sw, size_t sh, size_t stride, unsigned char *dst, size_t dw, size_t dh) {
    size_t wfrac = (sw << 12) / dw;
    size_t hfrac = (sh << 12) / dh;
    size_t darea = dw * dh;
    size_t sarea = sw * sh;
    int over, under;
    for (over = 0; (darea >> over) > sarea; over++);
    for (under = 0; (darea << under) < sarea; under++);
    size_t cscale = m::clamp(under, over - 12, 12),
    ascale = m::clamp(12 + under - over, 0, 24),
    dscale = ascale + 12 - cscale,
    area = ((unsigned long long)darea << ascale) / sarea;
    dw *= wfrac;
    dh *= hfrac;
    for (size_t y = 0; y < dh; y += hfrac) {
        const size_t yn = y + hfrac - 1;
        const size_t yi = y >> 12;
        const size_t h = (yn >> 12) - yi;
        const size_t ylow = ((yn | (-int(h) >> 24)) & 0xFFFU) + 1 - (y & 0xFFFU);
        const size_t yhigh = (yn & 0xFFFU) + 1;
        const unsigned char *ysrc = src + yi * stride;
        for (size_t x = 0; x < dw; x += wfrac, dst += S) {
            const size_t xn = x + wfrac - 1;
            const size_t xi = x >> 12;
            const size_t w = (xn >> 12) - xi;
            const size_t xlow = ((w + 0xFFFU) & 0x1000U) - (x & 0xFFFU);
            const size_t xhigh = (xn & 0xFFFU) + 1;
            const unsigned char *xsrc = ysrc + xi * S;
            const unsigned char *xend = xsrc + w * S;
            size_t r[S] = {0};
            for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                for (size_t i = 0; i < S; i++)
                    r[i] += xcur[i];
            for (size_t i = 0; i < S; i++)
                r[i] = (ylow * (r[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            if (h) {
                xsrc += stride;
                xend += stride;
                for (size_t hcur = h; --hcur; xsrc += stride, xend += stride) {
                    size_t p[S] = {0};
                    for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                        for (size_t i = 0; i < S; i++)
                            p[i] += xcur[i];
                    for (size_t i = 0; i < S; i++)
                        r[i] += ((p[i] << 12) + xsrc[i] * xlow + xend[i] * xhigh) >> cscale;
                }
                size_t p[S] = {0};
                for (const unsigned char *xcur = xsrc + S; xcur < xend; xcur += S)
                    for (size_t i = 0; i < S; i++)
                        p[i] += xcur[i];
                for (size_t i = 0; i < S; i++)
                    r[i] += (yhigh * (p[i] + ((xsrc[i] * xlow + xend[i] * xhigh) >> 12))) >> cscale;
            }
            for (size_t i = 0; i < S; i++)
                dst[i] = (r[i] * area) >> dscale;
        }
    }
}

static void scaleTexture(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t pitch, unsigned char *dst, size_t dw, size_t dh) {
    if (sw == dw * 2 && sh == dh * 2)
        switch (bpp) {
            case 3: return textureHalve<3>(src, sw, sh, pitch, dst);
            case 4: return textureHalve<4>(src, sw, sh, pitch, dst);
        }
    else if(sw < dw || sh < dh || sw&(sw-1) || sh&(sh-1) || dw&(dw-1) || dh&(dh-1))
        switch (bpp) {
            case 3: return textureScale<3>(src, sw, sh, pitch, dst, dw, dh);
            case 4: return textureScale<4>(src, sw, sh, pitch, dst, dw, dh);
        }
    switch(bpp) {
        case 3: return textureShift<3>(src, sw, sh, pitch, dst, dw, dh);
        case 4: return textureShift<4>(src, sw, sh, pitch, dst, dw, dh);
    }
}

static void reorientTexture(unsigned char *src, size_t sw, size_t sh, size_t bpp, size_t stride, unsigned char *dst, bool flipx, bool flipy, bool swapxy) {
    size_t stridex = swapxy ? bpp * sh : bpp;
    size_t stridey = swapxy ? bpp : bpp * sw;
    if (flipx)
        dst += (sw - 1) * stridex, stridex = -stridex;
    if (flipy)
        dst += (sh - 1) * stridey, stridey = -stridey;
    unsigned char *srcrow = src;
    for (size_t i = 0; i < sh; i++) {
        for (unsigned char *curdst = dst, *src = srcrow, *end = srcrow + sw * bpp; src < end; ) {
            for (size_t k = 0; k < bpp; k++)
                curdst[k] = *src++;
            curdst += stridex;
        }
        srcrow += stride;
        dst += stridey;
    }
}

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
#define R_TEX_DATA_RGB GL_UNSIGNED_BYTE
#define R_TEX_DATA_BGR GL_UNSIGNED_BYTE

static void getTextureFormat(const SDL_Surface *const surface, GLuint &dataFormat, GLuint &textureFormat) {
    if (surface->format->BytesPerPixel == 4) {
        if (surface->format->Rmask == 0x000000FF) {
            textureFormat = GL_RGBA;
            dataFormat = R_TEX_DATA_RGBA;
        } else {
            textureFormat = GL_BGRA;
            dataFormat = R_TEX_DATA_BGRA;
        }
    } else {
        if (surface->format->Rmask == 0x000000FF) {
            textureFormat = GL_RGB;
            dataFormat = R_TEX_DATA_RGB;
        } else {
            textureFormat = GL_BGR;
            dataFormat = R_TEX_DATA_BGR;
        }
    }
}

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

bool texture::load(const u::string &file) {
    // do not load multiple times
    if (m_loaded)
        return true;

    SDL_Surface *surface = IMG_Load(file.c_str());
    if (!surface)
        return false;

    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);

    GLuint dataFormat;
    GLuint textureFormat;
    getTextureFormat(surface, dataFormat, textureFormat);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, textureFormat,
        dataFormat, surface->pixels);

    glGenerateMipmap_(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    SDL_FreeSurface(surface);
    return m_loaded = true;
}

void texture::bind(GLenum unit) {
    glActiveTexture_(unit);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);
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

    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    int fw = 0;
    int fh = 0;
    for (size_t i = 0; i < 6; i++) {
        SDL_Surface *surface = IMG_Load(files[i].c_str());
        if (!surface)
            return false;

        GLuint dataFormat;
        GLuint textureFormat;
        getTextureFormat(surface, dataFormat, textureFormat);

        if (i == 0) {
            fw = surface->w;
            fh = surface->h;
        }

        // cubemap textures need be all the same size
        if (surface->w != fw || surface->h != fh) {
            unsigned char *scale = new unsigned char[fw * fh * surface->format->BytesPerPixel];
            scaleTexture((unsigned char *)surface->pixels, surface->w, surface->h,
                surface->format->BytesPerPixel, surface->w * surface->format->BytesPerPixel, scale, fw, fh);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, fw, fh, 0, textureFormat, dataFormat, scale);
        } else {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, surface->w, surface->h,
                0, textureFormat, dataFormat, surface->pixels);
        }

        SDL_FreeSurface(surface);
    }

    return m_loaded = true;
}

void textureCubemap::bind(GLenum unit) {
    glActiveTexture_(unit);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_textureHandle);
}

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
    GL_RESOLVE(glUniform3fv);
    GL_RESOLVE(glUniform4f);
    GL_RESOLVE(glGenerateMipmap);
    GL_RESOLVE(glDeleteShader);
    GL_RESOLVE(glGetShaderiv);
    GL_RESOLVE(glGetProgramiv);
    GL_RESOLVE(glGetShaderInfoLog);
    GL_RESOLVE(glActiveTexture);
}

void screenShot(const u::string &file) {
    SDL_Surface *temp = SDL_CreateRGBSurface(SDL_SWSURFACE, kScreenWidth, kScreenHeight,
        8*3, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_LockSurface(temp);
    unsigned char *pixels = new unsigned char[3 * kScreenWidth * kScreenHeight];
    glReadPixels(0, 0, kScreenWidth, kScreenHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    reorientTexture(pixels, kScreenWidth, kScreenHeight, 3, kScreenWidth * 3, (unsigned char *)temp->pixels, false, true, false);
    SDL_UnlockSurface(temp);
    delete[] pixels;
    SDL_SaveBMP(temp, file.c_str());
    SDL_FreeSurface(temp);
}
