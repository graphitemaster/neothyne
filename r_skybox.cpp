#include "c_console.h"

#include "r_skybox.h"
#include "r_pipeline.h"

#include "u_string.h"
#include "u_vector.h"
#include "u_misc.h"

#include "m_half.h"

namespace r {

///! method
bool skyboxMethod::init(const u::vector<const char *> &defines) {
    if (!method::init("skybox"))
        return false;

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/skybox.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/skybox.fs"))
        return false;
    if (!finalize({ "position" }))
        return false;

    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_world = getUniform("gWorld", uniform::kMat4);
    m_colorMap = getUniform("gColorMap", uniform::kSampler);
    m_skyColor = getUniform("gSkyColor", uniform::kVec3);

    // { { r, g, b }, { range.x, range.y, density } }
    m_fog0 = getUniform("gFog[0]", uniform::kVec3);
    m_fog1 = getUniform("gFog[1]", uniform::kVec3);
    m_fogEquation = getUniform("gFogEquation", uniform::kInt);

    post();
    return true;
}

void skyboxMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void skyboxMethod::setTextureUnit(int unit) {
    m_colorMap->set(unit);
}

void skyboxMethod::setWorld(const m::mat4 &worldInverse) {
    m_world->set(worldInverse);
}

void skyboxMethod::setFog(const fog &f) {
    m_fog0->set(f.color);
    m_fog1->set(m::vec3(f.start, f.end, f.density));
    m_fogEquation->set(int(f.equation));
}

void skyboxMethod::setSkyColor(const m::vec3 &skyColor) {
    m_skyColor->set(skyColor);
}

///! renderer
skybox::skybox()
    : m_vao(0)
    , m_vbo(0)
{
}

skybox::~skybox() {
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        gl::DeleteBuffers(1, &m_vbo);
}

bool skybox::load(const u::string &skyboxName) {
    auto &r_debug_tex = c::Console::value<int>("r_debug_tex");
    static const char *kSuffices[] = { "_bk", "_ft", "_lf", "_rt", "_up", "_dn" };
    for (size_t i = 0; i < 6; i++)
        if (!m_textures[i].load(skyboxName + kSuffices[i], false, false, r_debug_tex.get()))
            return false;

    // Calculate the average color of the top of the skybox. We utilize this color
    // for vertical fog mixture that reaches into the sky if the map has fog at
    // all.
    const auto &tex = m_textures[2].get();
    if (tex.flags() & kTexFlagCompressed) {
        // Unfortunately it's not possible to inherit the sky color from
        // texture data if it's a compressed block format.
        return false;
    }

    const auto &data = tex.data();

    // Assuming a maximum of 4096*4096 with 255 values per component, a
    // 32-bit unsigned integer is the perfect size to store this value!
    uint32_t totals[4] = {0};
    const size_t stride = tex.width() * tex.bpp();
    for (size_t y = 0; y < tex.height(); y++) {
        for (size_t x = 0; x < tex.width(); x++) {
            for (size_t i = 0; i < tex.bpp(); i++) {
                const size_t index = (y * stride) + x*tex.bpp() + i;
                totals[i] += data[index];
            }
        }
    }
    int reduce[4] = {0};
    for (size_t i = 0; i < tex.bpp(); i++)
        reduce[i] = totals[i] / (tex.width() * tex.height());

    if (tex.bpp() == 4) {
        // Do a cheap "alpha blend" of sorts to adjust the intensity of the
        // average color if there exists an alpha channel
        unsigned int alpha = reduce[3];
        unsigned int inv = 256 - alpha;
        reduce[0] = alpha * reduce[0] + inv * reduce[0];
        reduce[1] = alpha * reduce[1] + inv * reduce[1];
        reduce[2] = alpha * reduce[2] + inv * reduce[2];
    }

    m_skyColor = { reduce[0] / 255.0f,
                   reduce[1] / 255.0f,
                   reduce[2] / 255.0f };

    return true;
}

bool skybox::upload() {
    for (size_t i = 0; i < 6; i++)
        m_textures[i].upload(GL_CLAMP_TO_EDGE);

    alignas(16) static const float kData[] = {
        // Front
         1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f, 0.0f, 1.0f,
        // Back
        -1.0f,  1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
        // Left
        -1.0f,  1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
        // Right
         1.0f,  1.0f, -1.0f, 1.0f, 0.0f,
         1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f, 1.0f,
        // Top
        -1.0f,  1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f, 1.0f,
        // Bottom
         1.0f, -1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, -1.0f, 1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f, 0.0f, 1.0f
    };

    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(1, &m_vbo);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        const auto convert = m::convertToHalf(kData, sizeof kData / sizeof *kData);
        gl::BufferData(GL_ARRAY_BUFFER, convert.size() * sizeof convert[0], &convert[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, sizeof(m::half[5]), (const GLvoid *)0); // position
        gl::VertexAttribPointer(1, 2, GL_HALF_FLOAT, GL_FALSE, sizeof(m::half[5]), (const GLvoid *)sizeof(m::half[3])); // coordinate
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, sizeof kData, kData, GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float[5]), ATTRIB_OFFSET(0)); // position
        gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float[5]), ATTRIB_OFFSET(3)); // coordinate
    }
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);

    if (!m_methods[0].init())
        return false;
    if (!m_methods[1].init({"USE_FOG"}))
        return false;
    for (auto &it : m_methods) {
        it.enable();
        it.setTextureUnit(0);
        it.setSkyColor(m_skyColor);
    }
    return true;
}

void skybox::render(const pipeline &pl, const fog &f) {
    // Construct the matrix for the skybox
    pipeline p;
    p.setWorld(pl.position());
    p.setPosition(pl.position());
    p.setRotation(pl.rotation());
    p.setPerspective(pl.perspective());

    skyboxMethod *renderMethod = nullptr;
    if (c::Console::value<int>("r_fog")) {
        renderMethod = &m_methods[1];
        renderMethod->enable();
        renderMethod->setFog(f);
    } else {
        renderMethod = &m_methods[0];
        renderMethod->enable();
    }

    renderMethod->setWVP(p.projection() * p.view() * p.world());
    renderMethod->setWorld(pl.world());

    // render skybox cube
    gl::DepthRange(1, 1);
    gl::DepthFunc(GL_LEQUAL);
    gl::Disable(GL_BLEND);

    gl::BindVertexArray(m_vao);
    for (size_t i = 0; i < 6; i++) {
        m_textures[i].bind(GL_TEXTURE0);
        gl::DrawArrays(GL_TRIANGLE_STRIP, i*4, 4);
    }

    gl::DepthRange(0, 1);
    gl::DepthFunc(GL_LESS);
    gl::Enable(GL_BLEND);
}

}
