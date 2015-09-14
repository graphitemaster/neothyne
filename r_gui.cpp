#include <math.h> // sqrtf
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
    : m_screenSize(nullptr)
    , m_colorMap(nullptr)
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

    if (!finalize({ "position", "texCoord", "color" }))
        return false;

    m_screenSize = getUniform("gScreenSize", uniform::kVec2);
    m_colorMap = getUniform("gColorMap", uniform::kSampler);

    post();
    return true;
}

void guiMethod::setPerspective(const m::perspective &p) {
    m_screenSize->set(m::vec2(p.width, p.height));
}

void guiMethod::setColorTextureUnit(int unit) {
    m_colorMap->set(unit);
}

///! guiModelMethod
guiModelMethod::guiModelMethod()
    : m_WVP(nullptr)
    , m_world(nullptr)
    , m_colorTextureUnit(nullptr)
    , m_eyeWorldPosition(nullptr)
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
    if (!finalize({ "position", "normal", "texCoord", "tangent", "w" }))
        return false;

    m_WVP = getUniform("gWVP", uniform::kMat4);
    m_world = getUniform("gWorld", uniform::kMat4);
    m_eyeWorldPosition = getUniform("gEyeWorldPosition", uniform::kVec3);
    m_colorTextureUnit = getUniform("gColorMap", uniform::kSampler);

    post();
    return true;
}

void guiModelMethod::setWVP(const m::mat4 &wvp) {
    m_WVP->set(wvp);
}

void guiModelMethod::setWorld(const m::mat4 &world) {
    m_world->set(world);
}

void guiModelMethod::setColorTextureUnit(int unit) {
    m_colorTextureUnit->set(unit);
}

void guiModelMethod::setEyeWorldPos(const m::vec3 &pos) {
    m_eyeWorldPosition->set(pos);
}

///! gui::atlas
gui::atlas::atlas()
    : m_width(0)
    , m_height(0)
{
}

gui::atlas::node::node() {
    memset(this, 0, sizeof *this);
}

gui::atlas::node::~node() {
    delete l;
    delete r;
}

float gui::atlas::occupancy() const {
    const size_t total = m_width * m_height;
    const size_t used = usedSurfaceArea(m_root);
    return (float)used/total;
}

size_t gui::atlas::usedSurfaceArea(const gui::atlas::node &n) const {
    if (n.l || n.r) {
        size_t used = n.w * n.h;
        if (n.l)
            used += usedSurfaceArea(*n.l);
        if (n.r)
            used += usedSurfaceArea(*n.r);
        return used;
    }
    // Leaf node contains no surface area
    return 0;
}

gui::atlas::node *gui::atlas::insert(gui::atlas::node *n, int width, int height) {
    // Try both leaves for possible space
    if (n->l || n->r) {
        if (n->l) {
            node *newNode = insert(n->l, width, height);
            if (newNode)
                return newNode;
        }
        if (n->r) {
            node *newNode = insert(n->r, width, height);
            if (newNode)
                return newNode;
        }
        // Didn't fit into either subtree
        return nullptr;
    }

    // Can the new rectangle fit here?
    if (width > n->w || height > n->h)
        return nullptr;

    // Split remaining space along the shorter axis
    const int w = n->w - width;
    const int h = n->h - height;
    n->l = new node;
    n->r = new node;

    if (w <= h) {
        // Split the remaining space in horizontal direction
        n->l->x = n->x + width;
        n->l->y = n->y;
        n->l->w = w;
        n->l->h = height;
        n->r->x = n->x;
        n->r->y = n->y + height;
        n->r->w = n->w;
        n->r->h = h;
    } else {
        // Split the remaining space in vertical direction
        n->l->x = n->x;
        n->l->y = n->y + height;
        n->l->w = width;
        n->l->h = h;
        n->r->x = n->x + width;
        n->r->y = n->y;
        n->r->w = w;
        n->r->h = n->h;
    }

    // Note: as a result of the above technique, it can happen that n->l or
    // n->r becomes a degenerate (zero area) rectangle. There is no need to do
    // anything about it (such as removing the "unnecessary" nodes) as they need
    // to exist as children of this node (can't be a leaf anymore.)

    n->w = width;
    n->h = height;
    return n;
}

gui::atlas::node *gui::atlas::insert(int w, int h) {
    return insert(&m_root, w, h);
}

size_t gui::atlas::width() const {
    return m_width;
}

size_t gui::atlas::height() const {
    return m_height;
}

///! gui
static const size_t kAtlasSize = 1024;

gui::gui()
    : m_vbo(0)
    , m_vao(0)
    , m_notex(nullptr)
    , m_atlasData(new unsigned char[kAtlasSize*kAtlasSize*4])
    , m_atlasTexture(0)
{
    for (size_t i = 0; i < kCircleVertices; ++i) {
        const float a = float(i) / float(kCircleVertices) * m::kTau;
        float s, c;
        m::sincos(a, s, c);
        m_circleVertices[i*2+0] = c;
        m_circleVertices[i*2+1] = s;
    }

    m_atlas.m_width = kAtlasSize;
    m_atlas.m_height = kAtlasSize;
    m_atlas.m_root.w = kAtlasSize;
    m_atlas.m_root.h = kAtlasSize;
}


gui::~gui() {
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
    if (m_vbo)
        gl::DeleteBuffers(1, &m_vbo);
    for (auto &it : m_models)
        delete it.second;
    for (auto &it : m_modelTextures)
        delete it.second;

    gl::DeleteTextures(1, &m_atlasTexture);

#ifdef DEBUG_GUI
    ::texture saveAtlas;
    saveAtlas.from(m_atlasData, kAtlasSize*kAtlasSize*4, kAtlasSize, kAtlasSize,
        false, kTexFormatRGBA);
    if (saveAtlas.save("ui_atlas", kSaveTGA))
        u::print("wrote ui texture atlas to `ui_atlas.tga'\n");
#endif

    delete [] m_atlasData;

}

gui::atlas::node *gui::atlasPack(const u::string &file) {
    auto existing = m_textures.find(file);
    if (existing != m_textures.end())
        return existing->second;

    ::texture tex;
    if (!tex.load(file))
        return nullptr;

    auto node = m_atlas.insert(tex.width(), tex.height());
    if (!node)
        return nullptr;

    // Pack the data
    const unsigned char *data = tex.data();
    for (int H = node->h - 1; H >= 0; H--) {
        unsigned char *beg = m_atlasData + m_atlas.width()*4*(node->y+H) + node->x*4;
        for (int W = 0; W < node->w; W++) {
            // Convert all texture data into RGBA8
            switch (tex.format()) {
            case kTexFormatLuminance:
                *beg++ = *data;
                *beg++ = *data;
                *beg++ = *data;
                *beg++ = *data++;
                break;
            case kTexFormatBGR:
                *beg++ = data[2];
                *beg++ = data[1];
                *beg++ = data[0];
                *beg++ = 0xFF;
                data += 3;
                break;
            case kTexFormatBGRA:
                *beg++ = data[2];
                *beg++ = data[1];
                *beg++ = data[0];
                *beg++ = data[3];
                data += 4;
                break;
            case kTexFormatRG:
                *beg++ = *data++;
                *beg++ = *data++;
                *beg++ = 0;
                *beg++ = 0xFF;
                break;
            case kTexFormatRGB:
                *beg++ = *data++;
                *beg++ = *data++;
                *beg++ = *data++;
                *beg++ = 0xFF;
                break;
            case kTexFormatRGBA:
                *beg++ = *data++;
                *beg++ = *data++;
                *beg++ = *data++;
                *beg++ = *data++;
                break;
            default:
                assert(0 && "invalid texture format for UI atlas");
                break;
            }
        }
    }
    u::print("[atlas] => inserted (%zux%zu) at (%d,%d) (%.2f%% usage)\n",
        tex.width(), tex.height(), node->x, node->y, m_atlas.occupancy()*100.0f);
    return node;
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

    if (!(m_notex = atlasPack("textures/notex")))
        return false;

    return m_font.load("fonts/" + fontMap);
}

bool gui::upload() {
    if (!m_font.upload())
        return false;

    gl::GenTextures(1, &m_atlasTexture);
    gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kAtlasSize, kAtlasSize, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, m_atlasData);

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
                      it.asImage.path);
            break;
        case ::gui::kCommandModel:
            if (m_models.find(it.asModel.path) == m_models.end()) {
                auto mdl = u::unique_ptr<model>(new model);
                if (mdl->load(m_modelTextures, it.asModel.path) && mdl->upload())
                    m_models[it.asModel.path] = mdl.release();
            }
            break;
        }
    }

    if (m_batches.empty())
        return;

    // Blast it all out in one giant shot
    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);
    gl::BufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(vertex), &m_vertices[0], GL_DYNAMIC_DRAW);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(0));
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(2));
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), ATTRIB_OFFSET(4));

    bool rebind = true;
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
            if (b->method != method) {
                method = b->method;
                m_methods[method].enable();
            }
            if (it.type == ::gui::kCommandText) {
                m_font.bind(GL_TEXTURE0);
                rebind = true;
            } else if (rebind) {
                gl::ActiveTexture(GL_TEXTURE0);
                gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);
                rebind = false;
            }
            gl::DrawArrays(GL_TRIANGLES, b->start, b->count);
            b++;
        } else if (it.type == ::gui::kCommandModel) {
            rebind = true;
            gl::Enable(GL_DEPTH_TEST);
            gl::Clear(GL_DEPTH_BUFFER_BIT);
            m_modelMethod.enable();
            if (m_models.find(it.asModel.path) == m_models.end()) {
                gl::Disable(GL_DEPTH_TEST);
                gl::Viewport(0, 0, neoWidth(), neoHeight());
                continue;
            }
            auto &mdl = m_models[it.asModel.path];
            auto p = it.asModel.pipeline;
            gl::Viewport(it.asModel.x, it.asModel.y, it.asModel.w, it.asModel.h);
            m_modelMethod.setWorld(p.world());
            m_modelMethod.setWVP(p.projection() * p.view() * p.world());
            mdl->render();
            gl::Disable(GL_DEPTH_TEST);
            gl::Viewport(0, 0, neoWidth(), neoHeight());
            gl::BindVertexArray(m_vao);
        }
    }

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
    const int roundX = int(m::floor(xpos + b.xoff));
    const int roundY = int(m::floor(ypos - b.yoff));

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
    size_t size = 0;
    auto textLength = [this,&size](const char *contents) -> float {
        float position = 0;
        float length = 0;
        for (const char *ch = contents; *ch; ch++, size++) {
            if (*ch < 32 || m_glyphs.size() <= size_t(*ch - 32))
                continue;
            auto &b = m_glyphs[*ch - 32];
            const int round = int(m::floor(position + b.xoff) + 0.5f);
            length = round + b.x1 - b.x0;
            position += b.xadvance;
        }
        return length;
    };

    // Alignment of text
    if (align == ::gui::kAlignCenter)
        x -= textLength(contents) / 2;
    else if (align == ::gui::kAlignRight)
        x -= textLength(contents);

    const float R = float(color & 0xFF) / 255.0f;
    const float G = float((color >> 8) & 0xFF) / 255.0f;
    const float B = float((color >> 16) & 0xFF) / 255.0f;
    const float A = float((color >> 24) & 0xFF) / 255.0f;

    batch b;
    b.start = m_vertices.size();
    m_vertices.reserve(m_vertices.size() + 6 * size);
    for (const char *ch = contents; *ch; ch++) {
        auto quad = getGlyphQuad(m_font.width(), m_font.height(), *ch - 32, x, y);
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
    b.method = kMethodFont;
    m_batches.push_back(b);
}

void gui::drawImage(float x, float y, float w, float h, const char *path) {
    // Deal with loading of textures
    if (m_textures.find(path) == m_textures.end()) {
        auto insert = atlasPack(path);
        if (insert) {
            m_textures[path] = insert;
            // Update texture atlas data
            gl::BindTexture(GL_TEXTURE_2D, m_atlasTexture);
            gl::TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kAtlasSize, kAtlasSize,
                GL_RGBA, GL_UNSIGNED_BYTE, m_atlasData);
        } else {
            m_textures[path] = m_notex;
        }
    }

    batch b;
    auto tex = m_textures[path];

    float x1 = float(tex->x) / float(m_atlas.width());
    float y1 = float(tex->y) / float(m_atlas.height());
    float x2 = float(tex->x+tex->w) / float(m_atlas.width());
    float y2 = float(tex->y+tex->h) / float(m_atlas.height());

    b.start = m_vertices.size();
    m_vertices.reserve(m_vertices.size() + 6);

    m_vertices.push_back({x,   y,   x1, y1, 0,0,0,0});
    m_vertices.push_back({x+w, y+h, x2, y2, 0,0,0,0});
    m_vertices.push_back({x+w, y,   x2, y1, 0,0,0,0});
    m_vertices.push_back({x,   y,   x1, y1, 0,0,0,0});
    m_vertices.push_back({x,   y+h, x1, y2, 0,0,0,0});
    m_vertices.push_back({x+w, y+h, x2, y2, 0,0,0,0});

    b.count = m_vertices.size() - b.start;

    b.texture = tex;
    b.method = kMethodImage;
    m_batches.push_back(b);
}

}
