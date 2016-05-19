#include "r_light.h"
#include "r_skybox.h"

#include "m_mat.h"
#include "m_trig.h"

namespace r {

size_t pointLight::hash() const {
    return u::hash((const unsigned char *)this, sizeof *this);
}

size_t spotLight::hash() const {
    return pointLight::hash() ^ u::hash((const unsigned char *)this, sizeof *this);
}

///! Light Rendering Method
lightMethod::lightMethod()
    : m_WVP(nullptr)
    , m_inverse(nullptr)
    , m_colorTextureUnit(nullptr)
    , m_normalTextureUnit(nullptr)
    , m_depthTextureUnit(nullptr)
    , m_shadowMapTextureUnit(nullptr)
    , m_occlusionTextureUnit(nullptr)
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

    // { { r, g, b, ambient }, { dir.x, dir.y, dir.z, diffuse } }
    m_light0 = getUniform("gDirectionalLight[0]", uniform::kVec4);
    m_light1 = getUniform("gDirectionalLight[1]", uniform::kVec4);

    // { { r, g, b }, { range.x, range.y, density } }
    m_fog0 = getUniform("gFog[0]", uniform::kVec3);
    m_fog1 = getUniform("gFog[1]", uniform::kVec3);
    m_fogEquation = getUniform("gFogEquation", uniform::kInt);

    post();
    return true;
}

void directionalLightMethod::setLight(const directionalLight &light) {
    m_light0->set(m::vec4(light.color, light.ambient));
    m_light1->set(m::vec4(light.direction.normalized(), light.diffuse));
}

void directionalLightMethod::setFog(const fog &f) {
    m_fog0->set(f.color);
    m_fog1->set(m::vec3(f.start, f.end, f.density));
    m_fogEquation->set(int(f.equation));
}

///! Point Light Rendering Method
bool pointLightMethod::init(const u::vector<const char *> &defines) {
    if (!lightMethod::init("shaders/plight.vs",
                           "shaders/plight.fs",
                           "point lighting",
                           defines))
        return false;

    // { { r, g, b, diffuse }, { pos.x, pos.y, pos.z, radius } }
    m_light0 = getUniform("gPointLight[0]", uniform::kVec4);
    m_light1 = getUniform("gPointLight[1]", uniform::kVec4);

    m_lightWVP = getUniform("gLightWVP", uniform::kMat4);

    post();
    return true;
}

void pointLightMethod::setLight(const pointLight &light) {
    m_light0->set(m::vec4(light.color, light.diffuse));
    m_light1->set(m::vec4(light.position, light.radius));
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

    // { { r, g, b, diffuse }, { pos.x, pos.y, pos.z, radius }, { dir.x, dir.y, dir.z, cutoff } }
    m_light0 = getUniform("gSpotLight[0]", uniform::kVec4);
    m_light1 = getUniform("gSpotLight[1]", uniform::kVec4);
    m_light2 = getUniform("gSpotLight[2]", uniform::kVec4);
    m_lightWVP = getUniform("gLightWVP", uniform::kMat4);

    post();
    return true;
}

void spotLightMethod::setLight(const spotLight &light) {
    m_light0->set(m::vec4(light.color, light.diffuse));
    m_light1->set(m::vec4(light.position, light.radius));
    m_light2->set(m::vec4(light.direction.normalized(), m::cos(m::toRadian(light.cutOff))));
}

void spotLightMethod::setLightWVP(const m::mat4 &wvp) {
    m_lightWVP->set(wvp);
}

}
