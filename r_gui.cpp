#include <assert.h>

#include "engine.h"
#include "gui.h"

#include "r_gui.h"
#include "r_pipeline.h"
#include "r_model.h"

#include "u_file.h"
#include "u_misc.h"
#include "u_algorithm.h"

namespace r {

#ifdef DEBUG_GUI
static void printLine(const ::gui::line &it) {
    u::print("    [0] = { x: %d, y: %d }\n", it.x[0], it.y[0]);
    u::print("    [1] = { x: %d, y: %d }\n", it.x[1], it.y[0]);
    u::print("    r = %d\n", it.r);
}

static void printRectangle(const ::gui::rectangle &it) {
    u::print("    { x: %d, y: %d, w: %d, h: %d, r: %d }\n", it.x, it.y, it.w,
        it.h, it.r);
}

static void printText(const ::gui::text &it) {
    auto align = [](int a) {
        switch (a) {
            case ::gui::kAlignCenter: return "center";
            case ::gui::kAlignLeft:   return "left";
            case ::gui::kAlignRight:  return "right";
        }
        return "";
    };
    u::print("    { x: %d, y: %d, align: %s, contents: `%s' }\n", it.x, it.y,
        align(it.align), it.contents);
}

static void printScissor(const ::gui::scissor &it) {
    u::print("    { x: %d, y: %d, w: %d, h: %d }\n", it.x, it.y, it.w, it.h);
}

static void printTriangle(const ::gui::triangle &it) {
    u::print("    { x: %d, y: %d, w: %d, h: %d }\n", it.x, it.y, it.w, it.h);
}

static void printImage(const ::gui::image &it) {
    u::print("    { x: %d, y: %d, w: %d, h: %d, path: %s }\n",
        it.x, it.y, it.w, it.h, it.path);
}

static void printModel(const ::gui::model &it) {
    u::print("    { x: %d, y: %d, w: %d, h: %d, path: %s\n",
        it.x, it.y, it.w, it.h, it.path);
    u::print("      wvp: {\n");
    for (size_t i = 0; i < 4; i++) {
        u::print("          [%f, %f, %f, %f]\n",
            it.wvp.m[i][0], it.wvp.m[i][1], it.wvp.m[i][2], it.wvp.m[i][3]);
    }
    u::print("      }\n");
    u::print("    }\n");
}

static void printCommand(const ::gui::command &it) {
    switch (it.type) {
        case ::gui::kCommandLine:
            u::print("line:      (color: #%X)\n", it.color);
            printLine(it.asLine);
            break;
        case ::gui::kCommandRectangle:
            u::print("rectangle: (color: #%X)\n", it.color);
            printRectangle(it.asRectangle);
            break;
        case ::gui::kCommandScissor:
            u::print("scissor:\n");
            printScissor(it.asScissor);
            break;
        case ::gui::kCommandText:
            u::print("text:      (color: #%X)\n", it.color);
            printText(it.asText);
            break;
        case ::gui::kCommandTriangle:
            u::print("triangle:  (flags: %d | color: #%X)\n", it.flags, it.color);
            printTriangle(it.asTriangle);
            break;
        case ::gui::kCommandImage:
            u::print("image:\n");
            printImage(it.asImage);
        case ::gui::kCommandModel:
            u::print("model:\n");
            printModel(it.asModel);
            break;
    }
    u::print("\n");
}
#endif

///! guiMethod
guiMethod::guiMethod()
    : m_screenSizeLocation(-1)
    , m_colorMapLocation(-1)
{
}

bool guiMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/gui.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/gui.fs"))
        return false;

    if (!finalize())
        return false;

    m_screenSizeLocation = getUniformLocation("gScreenSize");
    m_colorMapLocation = getUniformLocation("gColorMap");

    return true;
}

void guiMethod::setPerspective(const m::perspective &p) {
    gl::Uniform2f(m_screenSizeLocation, p.width, p.height);
}

void guiMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

///! guiModelMethod
guiModelMethod::guiModelMethod()
    : m_WVPLocation(0)
    , m_colorTextureUnitLocation(0)
{
}

bool guiModelMethod::init(const u::vector<const char *> &defines) {
    if (!method::init())
        return false;

    for (auto &it : defines)
        method::define(it);

    if (!addShader(GL_VERTEX_SHADER, "shaders/guimodel.vs"))
        return false;
    if (!addShader(GL_FRAGMENT_SHADER, "shaders/guimodel.fs"))
        return false;

    if (!finalize())
        return false;

    m_WVPLocation = getUniformLocation("gWVP");
    m_worldLocation = getUniformLocation("gWorld");
    m_eyeWorldPositionLocation = getUniformLocation("gEyeWorldPosition");
    m_colorTextureUnitLocation = getUniformLocation("gColorMap");

    return true;
}

void guiModelMethod::setWVP(const m::mat4 &wvp) {
    gl::UniformMatrix4fv(m_WVPLocation, 1, GL_TRUE, (const GLfloat *)wvp.m);
}

void guiModelMethod::setWorld(const m::mat4 &world) {
    gl::UniformMatrix4fv(m_worldLocation, 1, GL_TRUE, (const GLfloat *)world.m);
}

void guiModelMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorTextureUnitLocation, unit);
}

void guiModelMethod::setEyeWorldPos(const m::vec3 &pos) {
    gl::Uniform3fv(m_eyeWorldPositionLocation, 1, (const GLfloat *)&pos.x);
}

///! gui
gui::gui()
    : m_vbo(0)
    , m_vao(0)
{
    for (size_t i = 0; i < kCircleVertices; ++i) {
        const float a = float(i) / float(kCircleVertices) * m::kTau;
        m_circleVertices[i*2+0] = cosf(a);
        m_circleVertices[i*2+1] = sinf(a);
    }
}

gui::~gui() {
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        gl::DeleteBuffers(1, &m_vbo);
    for (auto &it : m_textures) {
        if (it.second == &m_notex)
            continue;
        delete it.second;
    }
    for (auto &it : m_models)
        delete it.second;
}

bool gui::load(const u::string &font) {
    auto fp = u::fopen(neoGamePath() + font + ".cfg", "r");
    if (!fp)
        return false;

    u::string fontMap = "<grey>";
    while (auto get = u::getline(fp)) {
        auto &line = *get;
        auto contents = u::split(line);
        if (contents[0] == "font" && contents.size() == 2) {
            fontMap += contents[1];
            continue;
        }

        // Ignore anything which don't look like valid glyph entries
        if (contents.size() < 7)
            continue;

        glyph g;
        g.x0 = u::atoi(contents[0]);
        g.y0 = u::atoi(contents[1]);
        g.x1 = u::atoi(contents[2]);
        g.y1 = u::atoi(contents[3]);
        g.xoff = u::atof(contents[4]);
        g.yoff = u::atof(contents[5]);
        g.xadvance = u::atof(contents[6]);

        m_glyphs.push_back(g);
    }

    u::string where = "fonts/" + fontMap; // TODO: strip from font path
    if (!m_font.load(where))
        return false;

    return true;
}

bool gui::upload() {
    if (!m_font.upload())
        return false;

    // No texture needed if fail to load a menu texture
    if (!m_notex.load("textures/notex") || !m_notex.upload())
        return false;

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(1, &m_vbo);

    gl::BindVertexArray(m_vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertex), 0, GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(2));
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(4));

    // Rendering methods for GUI
    if (!m_methods[kMethodNormal].init())
        return false;
    if (!m_methods[kMethodFont].init({"HAS_FONT"}))
        return false;
    if (!m_methods[kMethodImage].init({"HAS_IMAGE"}))
        return false;
    if (!m_modelMethod.init())
        return false;

    m_methods[kMethodFont].enable();
    m_methods[kMethodFont].setColorTextureUnit(0);

    m_methods[kMethodImage].enable();
    m_methods[kMethodImage].setColorTextureUnit(0);

    m_modelMethod.enable();
    m_modelMethod.setColorTextureUnit(0);
    m_modelMethod.setEyeWorldPos(m::vec3::origin);

    return true;
}

void gui::render(const pipeline &pl) {
    auto perspective = pl.perspective();

    gl::Disable(GL_DEPTH_TEST);
    gl::Disable(GL_CULL_FACE);
    gl::Enable(GL_BLEND);
    gl::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_methods[kMethodNormal].enable();
    m_methods[kMethodNormal].setPerspective(perspective);
    m_methods[kMethodFont].enable();
    m_methods[kMethodFont].setPerspective(perspective);
    m_methods[kMethodImage].enable();
    m_methods[kMethodImage].setPerspective(perspective);

    for (auto &it : ::gui::commands()()) {
        assert(it.type != -1);
#ifdef DEBUG_GUI
        printCommand(it);
#endif
        switch (it.type) {
            case ::gui::kCommandRectangle:
                if (it.asRectangle.r == 0) {
                    drawRectangle(it.asRectangle.x,
                                  it.asRectangle.y,
                                  it.asRectangle.w,
                                  it.asRectangle.h,
                                  1.0f,
                                  it.color);
                } else {
                    drawRectangle(it.asRectangle.x,
                                  it.asRectangle.y,
                                  it.asRectangle.w,
                                  it.asRectangle.h,
                                  it.asRectangle.r,
                                  1.0f,
                                  it.color);
                }
                break;
            case ::gui::kCommandLine:
                drawLine(it.asLine.x[0], it.asLine.y[0],
                         it.asLine.x[1], it.asLine.y[1],
                         it.asLine.r,
                         1.0f,
                         it.color);
                break;
            case ::gui::kCommandTriangle:
                if (it.flags == 1) {
                    const float x = it.asTriangle.x;
                    const float y = it.asTriangle.y;
                    const float w = it.asTriangle.w;
                    const float h = it.asTriangle.h;
                    const float vertices[3 * 2] = {
                        x,     y,
                        x+w-1, y+h/2,
                        x,     y+h-1
                    };
                    drawPolygon(vertices, 1.0f, it.color);
                } else if (it.flags == 2) {
                    const float x = it.asTriangle.x;
                    const float y = it.asTriangle.y;
                    const float w = it.asTriangle.w;
                    const float h = it.asTriangle.h;
                    const float vertices[3 * 2] = {
                        x,     y+h-1,
                        x+w/2, y,
                        x+w-1, y+h-1
                    };
                    drawPolygon(vertices, 1.0f, it.color);
                }
                break;
            case ::gui::kCommandText:
                m_methods[kMethodFont].enable();
                m_methods[kMethodFont].setPerspective(perspective);
                drawText(it.asText.x, it.asText.y, it.asText.contents, it.asText.align, it.color);
                break;
            case ::gui::kCommandImage:
                m_methods[kMethodImage].enable();
                m_methods[kMethodImage].setPerspective(perspective);
                drawImage(it.asImage.x,
                          it.asImage.y,
                          it.asImage.w,
                          it.asImage.h,
                          it.asImage.path,
                          it.flags);
                break;
            case ::gui::kCommandModel:
                if (m_models.find(it.asModel.path) == m_models.end()) {
                    auto mdl = u::unique_ptr<model>(new model);
                    if (mdl->load(m_textures, it.asModel.path) && mdl->upload())
                        m_models[it.asModel.path] = mdl.release();
                }
                break;
        }
    }

    // Blast it all out in one giant shot
    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(2));
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(4));

    int method = -1;
    batch *b = &m_batches[0];
    gl::Disable(GL_SCISSOR_TEST);
    for (auto &it : ::gui::commands()()) {
        if (it.type == ::gui::kCommandScissor) {
            if (it.flags) {
                gl::Enable(GL_SCISSOR_TEST);
                gl::Scissor(it.asScissor.x, it.asScissor.y, it.asScissor.w, it.asScissor.h);
            } else {
                gl::Disable(GL_SCISSOR_TEST);
            }
        } else if (it.type != ::gui::kCommandModel) {
            if (b->texture)
                b->texture->bind(GL_TEXTURE0);
            if (b->method != method) {
                method = b->method;
                m_methods[method].enable();
            }
            gl::DrawArrays(GL_TRIANGLES, b->start, b->count);
            b++;
        }
    }

    // Render GUI models
    gl::Clear(GL_DEPTH_BUFFER_BIT);
    gl::Enable(GL_DEPTH_TEST);
    m_modelMethod.enable();
    for (auto &it : ::gui::commands()()) {
        if (it.type != ::gui::kCommandModel)
            continue;
        if (m_models.find(it.asModel.path) == m_models.end())
            continue;
        auto &mdl = m_models[it.asModel.path];
        auto p = it.asModel.pipeline;
        gl::Viewport(it.asModel.x, it.asModel.y, it.asModel.w, it.asModel.h);
        m_modelMethod.setWorld(p.world());
        m_modelMethod.setWVP(p.projection() * p.view() * p.world());
        mdl->mat.diffuse->bind(GL_TEXTURE0);
        mdl->render();
    }
    gl::Disable(GL_DEPTH_TEST);
    gl::Viewport(0, 0, neoWidth(), neoHeight());

    // Reset the batches and vertices each frame
    m_vertices.destroy();
    m_batches.destroy();

#ifdef DEBUG_GUI
    u::printf(">> COMPLETE GUI FRAME\n\n");
#endif

    gl::Enable(GL_DEPTH_TEST);
    gl::Enable(GL_CULL_FACE);
}

template <size_t E>
void gui::drawPolygon(const float (&coords)[E], float r, uint32_t color) {
    constexpr size_t numCoords = u::min(E/2, kCoordCount);

    // Normals
    for (size_t i = 0, j = numCoords - 1; i < numCoords; j = i++) {
        const float *v0 = &coords[j*2];
        const float *v1 = &coords[i*2];
        float dx = v1[0] - v0[0];
        float dy = v1[1] - v0[1];
        float distance = sqrtf(dx*dx + dy*dy);
        if (distance > 0.0f) {
            // Normalize distance
            distance = 1.0f / distance;
            dx *= distance;
            dy *= distance;
        }
        m_normals[j*2+0] = dy;
        m_normals[j*2+1] = -dx;
    }

    // Coordinates
    for (size_t i = 0, j = numCoords - 1; i < numCoords; j = i++) {
        const float dlx0 = m_normals[j*2+0];
        const float dly0 = m_normals[j*2+1];
        const float dlx1 = m_normals[i*2+0];
        const float dly1 = m_normals[i*2+1];
        float dmx = (dlx0 + dlx1) * 0.5f;
        float dmy = (dly0 + dly1) * 0.5f;
        float distance = sqrtf(dmx*dmx + dmy*dmy);
        if (distance > m::kEpsilon / 10) { // Smaller epsilon
            float scale = 1.0f / distance;
            if (scale > 10.0f)
                scale = 10.0f;
            dmx *= scale;
            dmy *= scale;
        }
        m_coords[i*2+0] = coords[i*2+0]+dmx*r;
        m_coords[i*2+1] = coords[i*2+1]+dmy*r;
    }

    float R = float(color & 0xFF) / 255.0f;
    float G = float((color >> 8) & 0xFF) / 255.0f;
    float B = float((color >> 16) & 0xFF) / 255.0f;
    float A = float((color >> 24) & 0xFF) / 255.0f;

    batch b;
    b.start = m_vertices.size();
    m_vertices.reserve(b.start + numCoords);
    for (size_t i = 0, j = numCoords - 1; i < numCoords; j = i++) {
        m_vertices.push_back({coords[i*2], coords[i*2+1], 0, 0, R,G,B,A});
        m_vertices.push_back({coords[j*2], coords[j*2+1], 0, 0, R,G,B,A});
        for (size_t k = 0; k < 2; k++)
            m_vertices.push_back({m_coords[j*2], m_coords[j*2+1], 0, 0, R,G,B,0.0f}); // No alpha
        m_vertices.push_back({m_coords[i*2], m_coords[i*2+1], 0, 0, R,G,B,0.0f}); // No alpha
        m_vertices.push_back({coords[i*2], coords[i*2+1], 0, 0, R,G,B,A});
    }
    for (size_t i = 2; i < numCoords; ++i) {
        m_vertices.push_back({coords[0],       coords[1],         0, 0, R,G,B,A});
        m_vertices.push_back({coords[(i-1)*2], coords[(i-1)*2+1], 0, 0, R,G,B,A});
        m_vertices.push_back({coords[i*2],     coords[i*2+1],     0, 0, R,G,B,A});
    }
    b.count = m_vertices.size() - b.start;
    b.method = kMethodNormal;
    b.texture = nullptr;
    m_batches.push_back(b);
}

void gui::drawRectangle(float x, float y, float w, float h, float fth, uint32_t color) {
    float vertices[4*2] = {
        x,   y,
        x+w, y,
        x+w, y+h,
        x,   y+h,
    };
    drawPolygon(vertices, fth, color);
}

void gui::drawRectangle(float x, float y, float w, float h, float r, float fth, uint32_t color) {
    const size_t kRound = kCircleVertices / 4;

    float vertices[(kRound+1)*4*2];
    float *v = vertices;

    for (size_t i = 0; i <= kRound; ++i) {
        *v++ = x+w-r+m_circleVertices[i*2+0]*r;
        *v++ = y+h-r+m_circleVertices[i*2+1]*r;
    }
    for (size_t i = kRound; i <= kRound*2; ++i) {
        *v++ = x+r+m_circleVertices[i*2+0]*r;
        *v++ = y+h-r+m_circleVertices[i*2+1]*r;
    }
    for (size_t i = kRound*2; i <= kRound*3; ++i) {
        *v++ = x+r+m_circleVertices[i*2+0]*r;
        *v++ = y+r+m_circleVertices[i*2+1]*r;
    }
    for (size_t i = kRound*3; i < kRound*4; ++i) {
        *v++ = x+w-r+m_circleVertices[i*2+0]*r;
        *v++ = y+r+m_circleVertices[i*2+1]*r;
    }
    *v++ = x+w-r+m_circleVertices[0]*r;
    *v++ = y+r+m_circleVertices[1]*r;

    drawPolygon(vertices, fth, color);
}

u::optional<gui::glyphQuad> gui::getGlyphQuad(int pw, int ph, size_t index, float &xpos, float &ypos) {
    // Prevent these situations
    if (m_glyphs.size() <= index)
        return u::none;

    auto &b = m_glyphs[index];
    const int roundX = int(floorf(xpos + b.xoff));
    const int roundY = int(floorf(ypos - b.yoff));

    glyphQuad q;
    q.x0 = float(roundX);
    q.y0 = float(roundY);
    q.x1 = float(roundX) + b.x1 - b.x0;
    q.y1 = float(roundY) - b.y1 + b.y0;

    q.s0 = b.x0 / float(pw);
    q.t0 = b.y0 / float(pw);
    q.s1 = b.x1 / float(ph);
    q.t1 = b.y1 / float(ph);

    xpos += b.xadvance;
    return q;
}

void gui::drawLine(float x0, float y0, float x1, float y1, float r, float fth, uint32_t color) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float distance = sqrtf(dx*dx + dy*dy);

    if (distance > m::kEpsilon) {
        distance = 1.0f / distance;
        dx *= distance;
        dy *= distance;
    }

    float nx = dy;
    float ny = -dx;

    r -= fth;
    r *= 0.5f;
    if (r < 0.01f)
        r = 0.01f;

    dx *= r;
    dy *= r;
    nx *= r;
    ny *= r;

    float vertices[4*2] = {
        x0-dx-nx, y0-dy-ny,
        x0-dx+nx, y0-dy+ny,
        x1+dx+nx, y1+dy+ny,
        x1+dx-nx, y1+dy-ny
    };

    drawPolygon(vertices, fth, color);
}

void gui::drawText(float x, float y, const char *contents, int align, uint32_t color) {
    // Calculate length of text
    const size_t size = strlen(contents);
    auto textLength = [this](const char *contents, size_t size) -> float {
        float position = 0;
        float length = 0;
        for (size_t i = 0; i < size; i++) {
            const int it = contents[i];
            if (it < 32 || m_glyphs.size() <= size_t(it - 32))
                continue;
            auto &b = m_glyphs[it - 32];
            const int round = int(floorf(position + b.xoff) + 0.5f);
            length = round + b.x1 - b.x0;
            position += b.xadvance;
        }
        return length;
    };

    // Alignment of text
    if (align == ::gui::kAlignCenter)
        x -= textLength(contents, size) / 2;
    else if (align == ::gui::kAlignRight)
        x -= textLength(contents, size);

    const float R = float(color & 0xFF) / 255.0f;
    const float G = float((color >> 8) & 0xFF) / 255.0f;
    const float B = float((color >> 16) & 0xFF) / 255.0f;
    const float A = float((color >> 24) & 0xFF) / 255.0f;

    batch b;
    b.start = m_vertices.size();
    m_vertices.reserve(m_vertices.size() + 6 * size);
    for (size_t i = 0; i < size; i++) {
        auto quad = getGlyphQuad(512, 512, contents[i] - 32, x, y);
        if (!quad)
            continue;
        const auto q = *quad;
        m_vertices.push_back({q.x0, q.y0, q.s0, q.t0, R,G,B,A});
        m_vertices.push_back({q.x1, q.y1, q.s1, q.t1, R,G,B,A});
        m_vertices.push_back({q.x1, q.y0, q.s1, q.t0, R,G,B,A});
        m_vertices.push_back({q.x0, q.y0, q.s0, q.t0, R,G,B,A});
        m_vertices.push_back({q.x0, q.y1, q.s0, q.t1, R,G,B,A});
        m_vertices.push_back({q.x1, q.y1, q.s1, q.t1, R,G,B,A});
    }
    b.count = m_vertices.size() - b.start;
    b.texture = &m_font;
    b.method = kMethodFont;
    m_batches.push_back(b);
}

void gui::drawImage(float x, float y, float w, float h, const char *path, bool mipmaps) {
    // Deal with loading of textures
    if (m_textures.find(path) == m_textures.end()) {
        auto tex = u::unique_ptr<texture2D>(new texture2D(mipmaps, kFilterBilinear));
        if (!tex->load(path) || !tex->upload())
            m_textures[path] = &m_notex;
        else
            m_textures[path] = tex.release();
    }

    batch b;
    b.start = m_vertices.size();
    m_vertices.reserve(m_vertices.size() + 6);
    m_vertices.push_back({x,   y,   0.0f, 1.0f, 0,0,0,0});
    m_vertices.push_back({x+w, y,   1.0f, 1.0f, 0,0,0,0});
    m_vertices.push_back({x+w, y+h, 1.0f, 0.0f, 0,0,0,0});
    m_vertices.push_back({x,   y,   0.0f, 1.0f, 0,0,0,0});
    m_vertices.push_back({x+w, y+h, 1.0f, 0.0f, 0,0,0,0});
    m_vertices.push_back({x,   y+h, 0.0f, 0.0f, 0,0,0,0});
    b.count = m_vertices.size() - b.start;
    b.texture = m_textures[path];
    b.method = kMethodImage;
    m_batches.push_back(b);
}

}
