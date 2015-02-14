#include "world.h"
#include "r_light.h"
#include "m_mat4.h"

namespace r {

///! Light Rendering Method
bool lightMethod::init(const char *vs, const char *fs, const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    for (auto &it : defines)
        method::define(it);

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
    m_occlusionTextureUnitLocation = getUniformLocation("gOcclusionMap");
    m_depthTextureUnitLocation = getUniformLocation("gDepthMap");

    // specular lighting
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");

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

void lightMethod::setDepthTextureUnit(int unit) {
    gl::Uniform1i(m_depthTextureUnitLocation, unit);
}

void lightMethod::setOcclusionTextureUnit(int unit) {
    gl::Uniform1i(m_occlusionTextureUnitLocation, unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, &position.x);
}

void lightMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
    gl::Uniform2f(m_screenFrustumLocation, p.nearp, p.farp);
}

///! Directional Light Rendering Method
bool directionalLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/dlight.vs", "shaders/dlight.fs", defines))
        return false;

    m_directionalLightLocation.color = getUniformLocation("gDirectionalLight.base.color");
    m_directionalLightLocation.ambient = getUniformLocation("gDirectionalLight.base.ambient");
    m_directionalLightLocation.diffuse = getUniformLocation("gDirectionalLight.base.diffuse");
    m_directionalLightLocation.direction = getUniformLocation("gDirectionalLight.direction");

    return true;
}

void directionalLightMethod::setLight(const directionalLight &light) {
    m::vec3 direction = light.direction.normalized();
    gl::Uniform3fv(m_directionalLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_directionalLightLocation.ambient, light.ambient);
    gl::Uniform3fv(m_directionalLightLocation.direction, 1, &direction.x);
    gl::Uniform1f(m_directionalLightLocation.diffuse, light.diffuse);
}

///! Point Light Rendering Method
bool pointLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/plight.vs", "shaders/plight.fs", defines))
        return false;

    m_pointLightLocation.color = getUniformLocation("gPointLight.base.color");
    m_pointLightLocation.ambient = getUniformLocation("gPointLight.base.ambient");
    m_pointLightLocation.diffuse = getUniformLocation("gPointLight.base.diffuse");
    m_pointLightLocation.position = getUniformLocation("gPointLight.position");
    m_pointLightLocation.radius = getUniformLocation("gPointLight.radius");

    return true;
}

void pointLightMethod::setLight(const pointLight &light) {
    gl::Uniform3fv(m_pointLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_pointLightLocation.ambient, light.ambient);
    gl::Uniform1f(m_pointLightLocation.diffuse, light.diffuse);
    gl::Uniform3fv(m_pointLightLocation.position, 1, &light.position.x);
    gl::Uniform1f(m_pointLightLocation.radius, light.radius);
}

///! Spot Light Rendering Method
bool spotLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/slight.vs", "shaders/slight.fs", defines))
        return false;

    m_spotLightLocation.color = getUniformLocation("gSpotLight.base.base.color");
    m_spotLightLocation.ambient = getUniformLocation("gSpotLight.base.base.ambient");
    m_spotLightLocation.diffuse = getUniformLocation("gSpotLight.base.base.diffuse");
    m_spotLightLocation.position = getUniformLocation("gSpotLight.base.position");
    m_spotLightLocation.radius = getUniformLocation("gSpotLight.base.radius");
    m_spotLightLocation.direction = getUniformLocation("gSpotLight.direction");
    m_spotLightLocation.cutOff = getUniformLocation("gSpotLight.cutOff");

    return true;
}

void spotLightMethod::setLight(const spotLight &light) {
    const float x = m::toRadian(light.direction.x);
    const float y = m::toRadian(light.direction.y);
    const float sx = sinf(x);
    const float sy = sinf(y);
    const float cx = cosf(x);
    const float cy = cosf(y);

    m::vec3 direction = m::vec3(sx, -(sy * cx), -(cy * cx)).normalized();

    gl::Uniform3fv(m_spotLightLocation.color, 1, &light.color.x);
    gl::Uniform1f(m_spotLightLocation.ambient, light.ambient);
    gl::Uniform1f(m_spotLightLocation.diffuse, light.diffuse);
    gl::Uniform3fv(m_spotLightLocation.position, 1, &light.position.x);
    gl::Uniform1f(m_spotLightLocation.radius, light.radius);
    gl::Uniform3fv(m_spotLightLocation.direction, 1, &direction.x);
    gl::Uniform1f(m_spotLightLocation.cutOff, cosf(m::toRadian(light.cutOff)));
}

}
