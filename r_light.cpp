#include "world.h"
#include "r_light.h"
#include "m_mat.h"

namespace r {

///! Light Rendering Method
lightMethod::lightMethod()
    : m_WVP(nullptr)
    , m_inverse(nullptr)
    , m_colorTextureUnit(nullptr)
    , m_normalTextureUnit(nullptr)
    , m_depthTextureUnit(nullptr)
    , m_shadowMapTextureUnit(nullptr)
    , m_eyeWorldPosition(nullptr)
    , m_screenSize(nullptr)
    , m_screenFrustum(nullptr)
{
}

bool lightMethod::init(const char *vs,
                       const char *fs,
                       const char *description,
                       const u::vector<const char *> &defines)
{
    if (!method::init(description))
        return false;

    if (gl::has(gl::ARB_texture_rectangle))
        method::define("HAS_TEXTURE_RECTANGLE");

    for (const auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, vs))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, fs))
        return false;
    if (!finalize({ "position" }))
        return false;

    // matrices
    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_inverse = getUniform("gInverse", uniform::kMat4);

    // samplers
    m_colorTextureUnit = getUniform("gColorMap", uniform::kSampler);
    m_normalTextureUnit = getUniform("gNormalMap", uniform::kSampler);
    m_occlusionTextureUnit = getUniform("gOcclusionMap", uniform::kSampler);
    m_depthTextureUnit = getUniform("gDepthMap", uniform::kSampler);
    m_shadowMapTextureUnit = getUniform("gShadowMap", uniform::kSampler);

    // specular lighting
    m_eyeWorldPosition = getUniform("gEyeWorldPosition", uniform::kVec3);

    // device uniforms
    m_screenSize = getUniform("gScreenSize", uniform::kVec2);
    m_screenFrustum = getUniform("gScreenFrustum", uniform::kVec2);

    post();
    return true;
}

void lightMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void lightMethod::setInverse(const m::mat4 &inverse) {
    m_inverse->set(inverse);
}

void lightMethod::setColorTextureUnit(int unit) {
    m_colorTextureUnit->set(unit);
}

void lightMethod::setNormalTextureUnit(int unit) {
    m_normalTextureUnit->set(unit);
}

void lightMethod::setDepthTextureUnit(int unit) {
    m_depthTextureUnit->set(unit);
}

void lightMethod::setShadowMapTextureUnit(int unit) {
    m_shadowMapTextureUnit->set(unit);
}

void lightMethod::setOcclusionTextureUnit(int unit) {
    m_occlusionTextureUnit->set(unit);
}

void lightMethod::setEyeWorldPos(const m::vec3 &position) {
    m_eyeWorldPosition->set(position);
}

void lightMethod::setPerspective(const m::perspective &p) {
    m_screenSize->set(m::vec2(p.width, p.height));
    m_screenFrustum->set(m::vec2(p.nearp, p.farp));
}

///! Directional Light Rendering Method
bool directionalLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/dlight.vs",
                           "shaders/dlight.fs",
                           "directional lighting",
                           defines))
        return false;

    m_directionalLight.color = getUniform("gDirectionalLight.base.color", uniform::kVec3);
    m_directionalLight.ambient = getUniform("gDirectionalLight.base.ambient", uniform::kFloat);
    m_directionalLight.diffuse = getUniform("gDirectionalLight.base.diffuse", uniform::kFloat);
    m_directionalLight.direction = getUniform("gDirectionalLight.direction", uniform::kVec3);

    m_fog.color = getUniform("gFog.color", uniform::kVec3);
    m_fog.density = getUniform("gFog.density", uniform::kFloat);
    m_fog.range = getUniform("gFog.range", uniform::kVec2);
    m_fog.equation = getUniform("gFog.equation", uniform::kInt);

    post();
    return true;
}

void directionalLightMethod::setLight(const directionalLight &light) {
    m_directionalLight.color->set(light.color);
    m_directionalLight.ambient->set(light.ambient);
    m_directionalLight.direction->set(light.direction.normalized());
    m_directionalLight.diffuse->set(light.diffuse);
}

void directionalLightMethod::setFog(const fog &f) {
    m_fog.color->set(f.color);
    m_fog.density->set(f.density);
    m_fog.range->set(m::vec2(f.start, f.end));
    m_fog.equation->set(int(f.equation));
}

///! Point Light Rendering Method
bool pointLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/plight.vs",
                           "shaders/plight.fs",
                           "point lighting",
                           defines))
        return false;

    m_pointLight.color = getUniform("gPointLight.base.color", uniform::kVec3);
    m_pointLight.ambient = getUniform("gPointLight.base.ambient", uniform::kFloat);
    m_pointLight.diffuse = getUniform("gPointLight.base.diffuse", uniform::kFloat);
    m_pointLight.position = getUniform("gPointLight.position", uniform::kVec3);
    m_pointLight.radius = getUniform("gPointLight.radius", uniform::kFloat);

    m_lightWVP = getUniform("gLightWVP", uniform::kMat4);

    post();
    return true;
}

void pointLightMethod::setLight(const pointLight &light) {
    m_pointLight.color->set(light.color);
    m_pointLight.ambient->set(light.ambient);
    m_pointLight.diffuse->set(light.diffuse);
    m_pointLight.position->set(light.position);
    m_pointLight.radius->set(light.radius);
}

void pointLightMethod::setLightWVP(const m::mat4 &wvp) {
    m_lightWVP->set(wvp);
}

///! Spot Light Rendering Method
bool spotLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/slight.vs",
                           "shaders/slight.fs",
                           "spot lighting",
                           defines))
        return false;

    m_spotLight.color = getUniform("gSpotLight.base.base.color", uniform::kVec3);
    m_spotLight.ambient = getUniform("gSpotLight.base.base.ambient", uniform::kFloat);
    m_spotLight.diffuse = getUniform("gSpotLight.base.base.diffuse", uniform::kFloat);
    m_spotLight.position = getUniform("gSpotLight.base.position", uniform::kVec3);
    m_spotLight.radius = getUniform("gSpotLight.base.radius", uniform::kFloat);
    m_spotLight.direction = getUniform("gSpotLight.direction", uniform::kVec3);
    m_spotLight.cutOff = getUniform("gSpotLight.cutOff", uniform::kFloat);

    m_lightWVP = getUniform("gLightWVP", uniform::kMat4);

    post();
    return true;
}

void spotLightMethod::setLight(const spotLight &light) {
    m_spotLight.color->set(light.color);
    m_spotLight.ambient->set(light.ambient);
    m_spotLight.diffuse->set(light.diffuse);
    m_spotLight.position->set(light.position);
    m_spotLight.radius->set(light.radius);
    m_spotLight.direction->set(light.direction.normalized());
    m_spotLight.cutOff->set(m::cos(m::toRadian(light.cutOff)));
}

void spotLightMethod::setLightWVP(const m::mat4 &wvp) {
    m_lightWVP->set(wvp);
}

}
