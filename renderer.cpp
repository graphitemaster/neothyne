#include <string.h>
#include "renderer.h"

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
        gl::DeleteShader(it);

    if (m_program)
        gl::DeleteProgram(m_program);
}

bool method::init(void) {
    m_program = gl::CreateProgram();
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
        printf("Shader error:\n%s\n", infoLog.c_str());
        return false;
    }

    gl::AttachShader(m_program, shaderObject);
    return true;
}

void method::enable(void) {
    gl::UseProgram(m_program);
}

GLint method::getUniformLocation(const char *name) {
    return gl::GetUniformLocation(m_program, name);
}

GLint method::getUniformLocation(const u::string &name) {
    return gl::GetUniformLocation(m_program, name.c_str());
}

bool method::finalize(void) {
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

gBuffer::gBuffer() :
    m_fbo(0),
    m_width(0),
    m_height(0)
{
    memset(m_textures, 0, sizeof(m_textures));
}

gBuffer::~gBuffer() {
    destroy();
}

void gBuffer::destroy(void) {
    gl::DeleteFramebuffers(1, &m_fbo);
    gl::DeleteTextures(kMax, m_textures);
}

void gBuffer::update(const m::perspectiveProjection &project) {
    size_t width = project.width;
    size_t height = project.height;

    if (m_width != width || m_height != height) {
        destroy();
        init(project);
    }
}

bool gBuffer::init(const m::perspectiveProjection &project) {
    m_width = project.width;
    m_height = project.height;

    gl::GenFramebuffers(1, &m_fbo);
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);

    gl::GenTextures(kMax, m_textures);

    // diffuse RGBA
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kDiffuse]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_width, m_height, 0, GL_RGB, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_textures[kDiffuse], 0);

    // normals
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kNormal]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, m_width, m_height, 0, GL_RG, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_textures[kNormal], 0);

    // depth
    gl::BindTexture(GL_TEXTURE_2D, m_textures[kDepth]);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D, m_textures[kDepth], 0);

    static GLenum drawBuffers[] = {
        GL_COLOR_ATTACHMENT0, // diffuse
        GL_COLOR_ATTACHMENT1  // normal
    };

    gl::DrawBuffers(kMax, drawBuffers);

    GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return false;

    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    return true;
}

void gBuffer::bindReading(void) {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    for (size_t i = 0; i < kMax; i++) {
        gl::ActiveTexture(GL_TEXTURE0 + i);
        gl::BindTexture(GL_TEXTURE_2D, m_textures[i]);
    }
}

void gBuffer::bindWriting(void) {
    gl::BindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
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

    // matrices
    m_WVPLocation = getUniformLocation("gWVP");
    m_inverseLocation = getUniformLocation("gInverse");

    // samplers
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");
    m_normalTextureUnitLocation = getUniformLocation("gNormalMap");
    m_depthTextureUnitLocation = getUniformLocation("gDepthMap");

    // specular lighting
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");
    m_matSpecularIntensityLocation = getUniformLocation("gMatSpecularIntensity");
    m_matSpecularPowerLocation = getUniformLocation("gMatSpecularPower");

    // device uniforms
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_screenFrustumLocation = getUniformLocation("gScreenFrustum");

    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat*)wvp.m);
}

void lightMethod::setInverse(const m::mat4 &inverse) {
    gl::UniformMatrix4fv(m_inverseLocation, 1, GL_TRUE, (const GLfloat*)inverse.m);
}

void lightMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void lightMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureUnitLocation, unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, &position.x);
}

void lightMethod::setMatSpecIntensity(float intensity) {
    gl::Uniform1f(m_matSpecularIntensityLocation, intensity);
}

void lightMethod::setMatSpecPower(float power) {
    gl::Uniform1f(m_matSpecularPowerLocation, power);
}

void lightMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
    gl::Uniform2f(m_screenFrustumLocation, project.nearp, project.farp);
}

void lightMethod::setDepthTextureUnit(int unit) {
    gl::Uniform1i(m_depthTextureUnitLocation, unit);
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
    gl::Uniform3fv(m_directionalLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_directionalLightLocation.ambient, light.ambient);
    gl::Uniform3fv(m_directionalLightLocation.direction, 1, &direction.x);
    gl::Uniform1f(m_directionalLightLocation.diffuse, light.diffuse);
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
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");
    m_normalTextureUnitLocation = getUniformLocation("gNormalMap");

    return true;
}

void geomMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void geomMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
}

void geomMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void geomMethod::setNormalTextureUnit(int unit) {
    gl::Uniform1i(m_normalTextureUnitLocation, unit);
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
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void skyboxMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_cubeMapLocation, unit);
}

void skyboxMethod::setWorld(const m::mat4 &worldInverse) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)worldInverse.m);
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
    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_timeLocation = getUniformLocation("gTime");

    return true;
}

void splashMethod::setScreenSize(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
}

void splashMethod::setTime(float dt) {
    gl::Uniform1f(m_timeLocation, dt);
}

void splashMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_splashTextureLocation, unit);
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
    gl::UniformMatrix4fv(m_VPLocation, 1, GL_TRUE, (const GLfloat*)vp.m);
}

void billboardMethod::setCamera(const m::vec3 &cameraPosition) {
    gl::Uniform3fv(m_cameraPositionLocation, 1, &cameraPosition.x);
}

void billboardMethod::setTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

void billboardMethod::setSize(float width, float height) {
    gl::Uniform2f(m_sizeLocation, width, height);
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
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

//// Renderers:

///! Skybox Renderer
skybox::~skybox(void) {
    gl::DeleteBuffers(2, m_buffers);
    gl::DeleteVertexArrays(1, &m_vao);
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
    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    gl::EnableVertexAttribArray(0);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    if (!skyboxMethod::init()) {
        fprintf(stderr, "failed initializing skybox rendering method\n");
        return false;
    }

    return true;
}

void skybox::render(const rendererPipeline &pipeline) {
    enable();

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
    gl::CullFace(GL_FRONT);

    setTextureUnit(0);
    m_cubemap.bind(GL_TEXTURE0); // bind cubemap texture

    gl::BindVertexArray(m_vao);
    gl::DrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_BYTE, (void *)0);
    gl::BindVertexArray(0);
    gl::CullFace(GL_BACK);
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
    gl::Disable(GL_CULL_FACE);
    gl::Disable(GL_DEPTH_TEST);

    enable();
    setTextureUnit(0);
    setScreenSize(pipeline.getPerspectiveProjection());
    setTime(dt);

    m_texture.bind(GL_TEXTURE0);
    m_quad.render();

    gl::Enable(GL_DEPTH_TEST);
    gl::Enable(GL_CULL_FACE);
}

///! Billboard Renderer
billboard::billboard() {

}

billboard::~billboard() {
    gl::DeleteBuffers(1, &m_vbo);
    gl::DeleteVertexArrays(1, &m_vao);
}

bool billboard::load(const u::string &billboardTexture) {
    if (!m_texture.load(billboardTexture))
        return false;
    return true;
}

bool billboard::upload(void) {
    if (!m_texture.upload())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(1, &m_vbo);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(m::vec3) * m_positions.size(), &m_positions[0], GL_STATIC_DRAW);

    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)0);
    gl::EnableVertexAttribArray(0);

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

    gl::BindVertexArray(m_vao);
    gl::DrawArrays(GL_POINTS, 0, m_positions.size());
    gl::BindVertexArray(0);
}

void billboard::add(const m::vec3 &position) {
    m_positions.push_back(position);
}

///! Quad Renderer
quad::~quad(void) {
    gl::DeleteBuffers(2, m_buffers);
}

bool quad::upload(void) {
    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);

    GLfloat vertices[] = {
        -1.0f,-1.0f, 0.0f, 0.0f,  0.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
         1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
         1.0f,-1.0f, 0.0f, 1.0f,  0.0f,
    };

    GLubyte indices[] = { 0, 1, 2, 0, 2, 3 };

    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)0); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, (void *)12); // uvs
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    return true;
}

void quad::render(void) {
    gl::BindVertexArray(m_vao);
    gl::DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
    gl::BindVertexArray(0);
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

    for (size_t i = 0; i < sizeof(places)/sizeof(*places); i++) {
        switch (rand() % 4) {
            case 0: m_billboards[kBillboardRail].add(places[i]); break;
            case 1: m_billboards[kBillboardLightning].add(places[i]); break;
            case 2: m_billboards[kBillboardRocket].add(places[i]); break;
            case 3: m_billboards[kBillboardShotgun].add(places[i]); break;
        }
    }
}

world::~world(void) {
    gl::DeleteBuffers(2, m_buffers);
    gl::DeleteVertexArrays(1, &m_vao);
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

    if (!m_directionalLightQuad.upload())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(kdBinVertex), &m_vertices[0], GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), 0);                 // vertex
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)12); // normals
    gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)24); // texCoord
    gl::VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)32); // tangent
    gl::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(kdBinVertex), (const GLvoid*)44); // w
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);
    gl::EnableVertexAttribArray(3);
    gl::EnableVertexAttribArray(4);

    // upload data
    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(GLuint), &m_indices[0], GL_STATIC_DRAW);

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

    // setup g-buffer
    if (!m_gBuffer.init(project)) {
        fprintf(stderr, "failed to initialize G-buffer\n");
        return false;
    }

    m_directionalLightMethod.enable();
    m_directionalLightMethod.setColorTextureUnit(gBuffer::kDiffuse);
    m_directionalLightMethod.setNormalTextureUnit(gBuffer::kNormal);
    m_directionalLightMethod.setDepthTextureUnit(gBuffer::kDepth);
    m_directionalLightMethod.setMatSpecIntensity(2.0f);
    m_directionalLightMethod.setMatSpecPower(200.0f);

    m_geomMethod.enable();
    m_geomMethod.setColorTextureUnit(0);
    m_geomMethod.setNormalTextureUnit(1);

    printf("[world] => uploaded\n");
    return true;
}

void world::geometryPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;

    m_geomMethod.enable();
    m_gBuffer.bindWriting();

    // Only the geometry pass should update the depth buffer
    gl::DepthMask(GL_TRUE);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Only the geometry pass should update the depth buffer
    gl::Enable(GL_DEPTH_TEST);
    gl::Disable(GL_BLEND);

    m_geomMethod.setWVP(p.getWVPTransform());
    m_geomMethod.setWorld(p.getWorldTransform());

    // Render the world
    gl::BindVertexArray(m_vao);
    for (auto &it : m_textureBatches) {
        it.diffuse->bind(GL_TEXTURE0);
        it.normal->bind(GL_TEXTURE1);
        gl::DrawElements(GL_TRIANGLES, it.count, GL_UNSIGNED_INT,
            (const GLvoid*)(sizeof(GLuint) * it.start));
    }

    // The depth buffer is populated and the stencil pass will require it.
    // However, it will not write to it.
    gl::DepthMask(GL_FALSE);
    gl::Disable(GL_DEPTH_TEST);
}

void world::beginLightPass(void) {
    gl::Enable(GL_BLEND);
    gl::BlendEquation(GL_FUNC_ADD);
    gl::BlendFunc(GL_ONE, GL_ONE);

    m_gBuffer.bindReading();
    gl::Clear(GL_COLOR_BUFFER_BIT);
}

void world::directionalLightPass(const rendererPipeline &pipeline) {
    const m::perspectiveProjection &project = pipeline.getPerspectiveProjection();
    m_directionalLightMethod.enable();

    m_directionalLightMethod.setDirectionalLight(m_directionalLight);
    m_directionalLightMethod.setPerspectiveProjection(project);

    m_directionalLightMethod.setEyeWorldPos(pipeline.getPosition());

    m::mat4 wvp;
    wvp.loadIdentity();

    rendererPipeline p = pipeline;
    m_directionalLightMethod.setWVP(wvp);
    m_directionalLightMethod.setInverse(p.getInverseTransform());
    m_directionalLightQuad.render();
}

void world::depthPass(const rendererPipeline &pipeline) {
    rendererPipeline p = pipeline;
    m_depthMethod.enable();
    m_depthMethod.setWVP(p.getWVPTransform());

    gl::Enable(GL_DEPTH_TEST); // make sure depth testing is enabled
    gl::Clear(GL_DEPTH_BUFFER_BIT); // clear

    // do a depth pass
    gl::BindVertexArray(m_vao);
    gl::DrawElements(GL_TRIANGLES, m_indices.size(), GL_UNSIGNED_INT, 0);
}

void world::depthPrePass(const rendererPipeline &pipeline) {
    gl::DepthMask(GL_TRUE);
    gl::DepthFunc(GL_LESS);
    gl::ColorMask(0.0f, 0.0f, 0.0f, 0.0f);

    depthPass(pipeline);

    gl::DepthFunc(GL_LEQUAL);
    gl::DepthMask(GL_FALSE);
    gl::ColorMask(1.0f, 1.0f, 1.0f, 1.0f);
}

void world::render(const rendererPipeline &pipeline) {
    m_gBuffer.update(pipeline.getPerspectiveProjection());

    // depth pre pass
    depthPrePass(pipeline);

    // geometry pass
    geometryPass(pipeline);

    // begin lighting pass
    beginLightPass();

    // directional light pass
    directionalLightPass(pipeline);

    // Now standard forward rendering
    //m_gBuffer.bindReading();
    gl::Enable(GL_DEPTH_TEST);
    m_skybox.render(pipeline);

    gl::DepthMask(GL_TRUE);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (auto &it : m_billboards)
        it.render(pipeline);
}
