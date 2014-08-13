#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "renderer.h"

static constexpr size_t kScreenWidth = 1600;
static constexpr size_t kScreenHeight = 852;

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
static PFNGLUNIFORM3FVPROC               glUniform3fv               = nullptr;
static PFNGLUNIFORM4FPROC                glUniform4f                = nullptr;
static PFNGLGENERATEMIPMAPPROC           glGenerateMipmap           = nullptr;
static PFNGLDELETESHADERPROC             glDeleteShader             = nullptr;
static PFNGLGETSHADERIVPROC              glGetShaderiv              = nullptr;
static PFNGLGETPROGRAMIVPROC             glGetProgramiv             = nullptr;
static PFNGLGETSHADERINFOLOGPROC         glGetShaderInfoLog         = nullptr;

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
        glDeleteShader(it);

    if (m_program)
        glDeleteProgram(m_program);
}

bool method::init(void) {
    m_program = glCreateProgram();
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

    GLuint shaderObject = glCreateShader(shaderType);
    if (!shaderObject)
        return false;

    m_shaders.push_back(shaderObject);

    const GLchar *shaderSources[1];
    GLint shaderLengths[1];

    shaderSources[0] = (GLchar *)&*shaderSource->begin();
    shaderLengths[0] = (GLint)shaderSource->size();

    glShaderSource(shaderObject, 1, shaderSources, shaderLengths);
    glCompileShader(shaderObject);

    GLint shaderCompileStatus = 0;
    glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &shaderCompileStatus);
    if (shaderCompileStatus == 0) {
        u::string infoLog;
        GLint infoLogLength = 0;
        glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &infoLogLength);
        infoLog.resize(infoLogLength);
        glGetShaderInfoLog(shaderObject, infoLogLength, nullptr, &*infoLog.begin());
        printf("Shader error:\n%s\n", infoLog.c_str());
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
    glUniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void lightMethod::setWorld(const m::mat4 &worldInverse) {
    glUniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void lightMethod::setTextureUnit(int unit) {
    glUniform1i(m_samplerLocation, unit);
}

void lightMethod::setNormalUnit(int unit) {
    glUniform1i(m_normalMapLocation, unit);
}

void lightMethod::setDirectionalLight(const directionalLight &light) {
    m::vec3 direction = light.direction;
    direction.normalize();

    glUniform3fv(m_directionalLight.colorLocation, 1, &light.color.x);
    glUniform3fv(m_directionalLight.directionLocation, 1, &direction.x);
    glUniform1f(m_directionalLight.ambientLocation, light.ambient);
    glUniform1f(m_directionalLight.diffuseLocation, light.diffuse);
}

void lightMethod::setPointLights(u::vector<pointLight> &lights) {
    // limit the number of maximum lights
    if (lights.size() > kMaxPointLights)
        lights.resize(kMaxPointLights);

    glUniform1i(m_numPointLightsLocation, lights.size());

    for (size_t i = 0; i < lights.size(); i++) {
        glUniform3fv(m_pointLights[i].colorLocation, 1, &lights[i].color.x);
        glUniform3fv(m_pointLights[i].positionLocation, 1, &lights[i].position.x);
        glUniform1f(m_pointLights[i].ambientLocation, lights[i].ambient);
        glUniform1f(m_pointLights[i].diffuseLocation, lights[i].diffuse);
        glUniform1f(m_pointLights[i].attenuation.constantLocation, lights[i].attenuation.constant);
        glUniform1f(m_pointLights[i].attenuation.linearLocation, lights[i].attenuation.linear);
        glUniform1f(m_pointLights[i].attenuation.expLocation, lights[i].attenuation.exp);
    }
}

void lightMethod::setEyeWorldPos(const m::vec3 &eyeWorldPos) {
    glUniform3fv(m_eyeWorldPosLocation, 1, &eyeWorldPos.x);
}

void lightMethod::setMatSpecIntensity(float intensity) {
    glUniform1f(m_matSpecIntensityLocation, intensity);
}

void lightMethod::setMatSpecPower(float power) {
    glUniform1f(m_matSpecPowerLocation, power);
}

void lightMethod::setFogType(fogType type) {
    glUniform1i(m_fog.methodLocation, type);
}

void lightMethod::setFogDistance(float start, float end) {
    glUniform1f(m_fog.startLocation, start);
    glUniform1f(m_fog.endLocation, end);
}

void lightMethod::setFogDensity(float density) {
    glUniform1f(m_fog.densityLocation, density);
}

void lightMethod::setFogColor(const m::vec3 &color) {
    glUniform4f(m_fog.colorLocation, color.x, color.y, color.z, 1.0f);
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
    m_cubeMapLocation = getUniformLocation("gCubemap");

    return true;
}

void skyboxMethod::setWVP(const m::mat4 &wvp) {
    glUniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void skyboxMethod::setTextureUnit(int unit) {
    glUniform1i(m_cubeMapLocation, unit);
}

///! skybox renderer
skybox::~skybox(void) {
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
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
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // generate the vbo and ibo
    glGenBuffers(2, m_buffers);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

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
    glUniform1f(m_aspectRatioLocation, (float)kScreenWidth / (float)kScreenHeight);
}

void splashMethod::setTextureUnit(int unit) {
    glUniform1i(m_splashTextureLocation, unit);
}

void skybox::render(const rendererPipeline &pipeline) {
    enable();

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

    setWVP(p.getWVPTransform());

    // render skybox cube

    glDisable(GL_BLEND);
    glCullFace(GL_FRONT);
    glDepthFunc(GL_LEQUAL);

    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture
    setTextureUnit(0);

    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_SHORT, (void *)0);

    glCullFace(faceMode);
    glDepthFunc(depthMode);
    glEnable(GL_BLEND);

    glUseProgram(0);
}

///! splash screen renderer
splashScreen::~splashScreen(void) {
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
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

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(2, m_buffers);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    GLfloat vertices[] = {
         1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
         1.0f,-1.0f, 0.0f, 1.0f,  0.0f,
        -1.0f,-1.0f, 0.0f, 0.0f,  0.0f,
    };

    GLushort indices[] = { 0, 1, 2, 2, 1, 3 };

    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)0); // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)12); // uvs
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    return true;
}

void splashScreen::render(void) {
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    enable();
    setTextureUnit(0);
    setAspectRatio();

    m_texture.bind(GL_TEXTURE0);

    glBindVertexArray(m_vao);
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

    if (!lightMethod::init()) {
        fprintf(stderr, "failed to initialize rendering method\n");
        abort();
    }

    if (!m_skybox.init("textures/sky01")) {
        fprintf(stderr, "failed to load skybox\n");
        abort();
    }
}

world::~world(void) {
    glDeleteBuffers(2, m_buffers);
    glDeleteVertexArrays(1, &m_vao);
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
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // gen vbo and ibo
    glGenBuffers(2, m_buffers);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, map.vertices.size() * sizeof(kdBinVertex), &*map.vertices.begin(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &*indices.begin(), GL_STATIC_DRAW);

    printf("textures:\n");
    printf("loaded: %zu\n", m_textureManager.loaded());
    printf("shared: %zu\n", m_textureManager.reuses());

    return true;
}

void world::draw(const rendererPipeline &pipeline) {
    rendererPipeline worldPipeline = pipeline;
    rendererPipeline skyPipeline = pipeline;

    const m::mat4 wvp = worldPipeline.getWVPTransform();
    const m::mat4 worldTransform = worldPipeline.getWorldTransform();
    const m::vec3 &pos = worldPipeline.getPosition();

    enable();
    setTextureUnit(0);
    setNormalUnit(1);

    static bool setFog = true;
    if (setFog) {
        setFogColor(m::vec3(0.8f, 0.8f, 0.8f));
        setFogDensity(0.0007f);
        setFogType(kFogExp);
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

    setDirectionalLight(m_directionalLight);

    setPointLights(m_pointLights);

    setWVP(wvp);
    setWorld(worldTransform);

    setEyeWorldPos(pos);
    setMatSpecIntensity(2.0f);
    setMatSpecPower(200.0f);

    glBindVertexArray(m_vao);
    for (size_t i = 0; i < m_textureBatches.size(); i++) {
        m_textureBatches[i].diffuse->bind(GL_TEXTURE0);
        m_textureBatches[i].normal->bind(GL_TEXTURE1);
        glDrawElements(GL_TRIANGLES, m_textureBatches[i].count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(uint32_t) * m_textureBatches[i].start));
    }
    glBindVertexArray(0);

    m_skybox.render(skyPipeline);
}

// GL_TEXTURE_2D
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

    GLuint format;
    if (surface->format->BytesPerPixel == 4)
        format = surface->format->Rmask == 0x000000FF ? GL_RGBA : GL_BGRA;
    else
        format = surface->format->Rmask == 0x000000FF ? GL_RGB : GL_BGR;

    glGenTextures(1, &m_textureHandle);
    glBindTexture(GL_TEXTURE_2D, m_textureHandle);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, format, GL_UNSIGNED_BYTE, surface->pixels);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    SDL_FreeSurface(surface);
    return m_loaded = true;
}

void texture::bind(GLenum unit) {
    glActiveTexture(unit);
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

    for (size_t i = 0; i < 6; i++) {
        SDL_Surface *surface = IMG_Load(files[i].c_str());
        if (!surface)
            return false;

        GLuint format;
        if (surface->format->BytesPerPixel == 4)
            format = surface->format->Rmask == 0x000000FF ? GL_RGBA : GL_BGRA;
        else
            format = surface->format->Rmask == 0x000000FF ? GL_RGB : GL_BGR;

        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, surface->w, surface->h,
            0, format, GL_UNSIGNED_BYTE, surface->pixels);

        SDL_FreeSurface(surface);
    }

    return m_loaded = true;
}

void textureCubemap::bind(GLenum unit) {
    glActiveTexture(unit);
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
    *(void **)&glUniform3fv               = SDL_GL_GetProcAddress("glUniform3fv");
    *(void **)&glUniform4f                = SDL_GL_GetProcAddress("glUniform4f");
    *(void **)&glGenerateMipmap           = SDL_GL_GetProcAddress("glGenerateMipmap");
    *(void **)&glDeleteShader             = SDL_GL_GetProcAddress("glDeleteShader");
    *(void **)&glGetShaderiv              = SDL_GL_GetProcAddress("glGetShaderiv");
    *(void **)&glGetProgramiv             = SDL_GL_GetProcAddress("glGetProgramiv");
    *(void **)&glGetShaderInfoLog         = SDL_GL_GetProcAddress("glGetShaderInfoLog");
}

void screenShot(const u::string &file) {
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
