#include "r_gui.h"

#include "u_file.h"
#include "u_misc.h"

#include "engine.h"

namespace r {

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

void guiMethod::setPerspectiveProjection(const m::perspectiveProjection &project) {
    gl::Uniform2f(m_screenSizeLocation, project.width, project.height);
}

void guiMethod::setColorTextureUnit(int unit) {
    gl::Uniform1i(m_colorMapLocation, unit);
}

gui::gui() {
    for (size_t i = 0; i < kCircleVertices; ++i) {
        const float a = float(i) / float(kCircleVertices) * m::kTau;
        m_circleVertices[i*2+0] = cosf(a);
        m_circleVertices[i*2+1] = sinf(a);
    }
}

gui::~gui() {
    if (m_white)
        gl::DeleteTextures(1, &m_white);
    if (m_vao)
        gl::DeleteVertexArrays(1, &m_vao);
    if (m_vbos[0])
        gl::DeleteBuffers(3, m_vbos);
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
    u::print("%s\n", where);
    if (!m_font.load(where))
        return false;

    return true;
}

bool gui::upload() {
    if (!m_font.upload())
        return false;

    unsigned char whiteAlpha = 255;
    gl::GenTextures(1, &m_white);
    gl::BindTexture(GL_TEXTURE_2D, m_white);
    gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &whiteAlpha);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl::GenVertexArrays(1, &m_vao);
    gl::GenBuffers(3, m_vbos);

    gl::BindVertexArray(m_vao);
    gl::EnableVertexAttribArray(0);
    gl::EnableVertexAttribArray(1);
    gl::EnableVertexAttribArray(2);

    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[0]);
    gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat[2]), nullptr);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[1]);
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat[2]), nullptr);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[2]);
    gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GLfloat[4]), nullptr);

    gl::BufferData(GL_ARRAY_BUFFER, 0, 0, GL_STATIC_DRAW);

    // Rendering method for GUI
    if (!m_methods[kMethodNormal].init())
        return false;
    if (!m_methods[kMethodFont].init({"HAS_FONT"}))
        return false;

    m_methods[kMethodFont].enable();
    m_methods[kMethodFont].setColorTextureUnit(0);

    return true;
}

void gui::render(const rendererPipeline &pipeline) {
    auto project = pipeline.getPerspectiveProjection();

    const float kScale = 1.0f / 8.0f;

    gl::Disable(GL_DEPTH_TEST);
    gl::Disable(GL_CULL_FACE);
    gl::Enable(GL_BLEND);

    m_methods[kMethodNormal].enable();
    m_methods[kMethodNormal].setPerspectiveProjection(project);

    gl::Disable(GL_SCISSOR_TEST);
    for (auto &it : ::gui::commands()) {
        switch (it.type) {
            case ::gui::kCommandRectangle:
                if (it.asRectangle.r == 0) {
                    drawRectangle(float(it.asRectangle.x) * kScale + 0.5f,
                                  float(it.asRectangle.y) * kScale + 0.5f,
                                  float(it.asRectangle.w) * kScale - 1,
                                  float(it.asRectangle.h) * kScale - 1,
                                  1.0f,
                                  it.color);
                } else {
                    drawRectangle(float(it.asRectangle.x) * kScale + 0.5f,
                                  float(it.asRectangle.y) * kScale + 0.5f,
                                  float(it.asRectangle.w) * kScale - 1,
                                  float(it.asRectangle.h) * kScale - 1,
                                  float(it.asRectangle.r) * kScale,
                                  1.0f,
                                  it.color);
                }
                break;
            case ::gui::kCommandLine:
                drawLine(it.asLine.x[0] * kScale, it.asLine.y[0] * kScale,
                         it.asLine.x[1] * kScale, it.asLine.y[1] * kScale,
                         it.asLine.r * kScale,
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
                        x*kScale+0.5f,                y*kScale+0.5f,
                        x*kScale+0.5f+w*kScale-1,     y*kScale+0.5f+h*kScale/2-0.5f,
                        x*kScale+0.5f,                y*kScale+0.5f+h*kScale-1
                    };
                    drawPolygon(vertices, 3, 1.0f, it.color);
                } else if (it.flags == 2) {
                    const float x = it.asTriangle.x;
                    const float y = it.asTriangle.y;
                    const float w = it.asTriangle.w;
                    const float h = it.asTriangle.h;
                    const float vertices[3 * 2] = {
                        x*kScale+0.5f,                 y*kScale+0.5f+h*kScale-1,
                        x*kScale+0.5f+w*kScale/2-0.5f, y*kScale+0.5f,
                        x*kScale+0.5f+w*kScale-1,      y*kScale+0.5f+h*kScale-1
                    };
                    drawPolygon(vertices, 3, 1.0f, it.color);
                }
                break;
            case ::gui::kCommandText:
                m_methods[kMethodFont].enable();
                m_methods[kMethodFont].setPerspectiveProjection(project);
                drawText(it.asText.x, it.asText.y, it.asText.contents, it.asText.align, it.color);
                break;
            case ::gui::kCommandScissor:
                if (it.flags) {
                    gl::Enable(GL_SCISSOR_TEST);
                    gl::Scissor(it.asScissor.x, it.asScissor.y, it.asScissor.w, it.asScissor.h);
                } else {
                    gl::Disable(GL_SCISSOR_TEST);
                }
                break;
        }
    }

    gl::Enable(GL_DEPTH_TEST);
    gl::Enable(GL_CULL_FACE);
}

void gui::drawPolygon(const float *coords, size_t numCoords, float r, uint32_t color) {
    if (numCoords > kCoordCount)
        numCoords = kCoordCount;

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
        if (distance > 0.000001f) { // Smaller epsilon
            float scale = 1.0f / distance;
            if (scale > 10.0f)
                scale = 10.0f;
            dmx *= scale;
            dmy *= scale;
        }
        m_coords[i*2+0] = coords[i*2+0]+dmx*r;
        m_coords[i*2+1] = coords[i*2+1]+dmy*r;
    }

    const float R = float(color & 0xFF) / 255.0f;
    const float G = float((color >> 8) & 0xFF) / 255.0f;
    const float B = float((color >> 16) & 0xFF) / 255.0f;
    const float A = float((color >> 24) & 0xFF) / 255.0f;

    const size_t verticesCount = numCoords * 12 + (numCoords - 2) * 6;
    const size_t coordsCount = numCoords * 2 * 6 + (numCoords - 2) * 2 * 3;
    const size_t colorsCount = numCoords * 4 * 6 + (numCoords - 2) * 4 * 3;

    memset(m_textureCoords, 0, coordsCount * sizeof(float));
    memset(m_colors, 1, colorsCount * sizeof(float));

    // Vertices / Colors
    float *v = m_vertices;
    float *c = m_colors;
    for (size_t i = 0, j = numCoords - 1; i < numCoords; j = i++) {
        *v++ = coords[i*2+0];
        *v++ = coords[i*2+1];
        *v++ = coords[j*2+0];
        *v++ = coords[j*2+1];
        for (size_t k = 0; k < 2; k++) {
            *v++ = m_coords[j*2+0];
            *v++ = m_coords[j*2+1];
        }
        *v++ = m_coords[i*2+0];
        *v++ = m_coords[i*2+1];
        *v++ = coords[i*2+0];
        *v++ = coords[i*2+1];
        for (size_t k = 0; k < 2; k++) {
            *c++ = R;
            *c++ = G;
            *c++ = B;
            *c++ = A;
        }
        for (size_t k = 0; k < 3; k++) {
            *c++ = R;
            *c++ = G;
            *c++ = B;
            *c++ = 0.0f; // No alpha
        }
        *c++ = R;
        *c++ = G;
        *c++ = B;
        *c++ = A;
    }
    for (size_t i = 2; i < numCoords; ++i) {
        *v++ = coords[0];
        *v++ = coords[1];
        *v++ = coords[(i-1)*2+0];
        *v++ = coords[(i-1)*2+1];
        *v++ = coords[i*2+0];
        *v++ = coords[i*2+1];
        for (size_t j = 0; j < 3; j++) {
            *c++ = R;
            *c++ = G;
            *c++ = B;
            *c++ = A;
        }
    }

    gl::BindTexture(GL_TEXTURE_2D, m_white);

    gl::BindVertexArray(m_vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[0]);
    gl::BufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(float), m_vertices, GL_STATIC_DRAW);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[1]);
    gl::BufferData(GL_ARRAY_BUFFER, coordsCount * sizeof(float), m_textureCoords, GL_STATIC_DRAW);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[2]);
    gl::BufferData(GL_ARRAY_BUFFER, colorsCount * sizeof(float), m_colors, GL_STATIC_DRAW);

    gl::DrawArrays(GL_TRIANGLES, 0, (numCoords * 2 + numCoords - 2) * 3);
}

void gui::drawRectangle(float x, float y, float w, float h, float fth, uint32_t color) {
    float vertices[4*2] = {
        x+0.5f,   y+0.5f,
        x+w-0.5f, y+0.5f,
        x+w-0.5f, y+h-0.5f,
        x+0.5f,   y+h-0.5f,
    };
    drawPolygon(vertices, 4, fth, color);
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

    drawPolygon(vertices, (kRound+1)*4, fth, color);
}

gui::glyphQuad gui::getGlyphQuad(int pw, int ph, size_t index, float &xpos, float &ypos) {
    glyphQuad q;
    auto &b = m_glyphs[index];
    const int roundX = int(floorf(xpos + b.xoff));
    const int roundY = int(floorf(ypos - b.yoff));

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

    if (distance > 0.0001f) {
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

    drawPolygon(vertices, 4, fth, color);
}

void gui::drawText(float x, float y, const u::string &contents, int align, uint32_t color) {
    // Calculate length of text
    auto textLength = [this](const u::string &contents) -> float {
        float position = 0;
        float length = 0;
        for (int it : contents) {
            // Ignore anything not ASCII
            if (it < 32 || it > 128)
                continue;
            auto &b = m_glyphs[it - 32];
            const int round = int(floorf(position + b.xoff) + 0.5f);
            length = round + b.x1 - b.x0 + 0.5f;
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

    m_font.bind(GL_TEXTURE0);

    for (int it : contents) {
        // Ignore anything not ASCII
        if (it < 32 || it > 128)
            continue;
        // Construct a quad for the glyph
        auto q = getGlyphQuad(512, 512, it - 32, x, y);

        // Data for GL
        float v[12] = {
            q.x0, q.y0, q.x1, q.y1,
            q.x1, q.y0, q.x0, q.y0,
            q.x0, q.y1, q.x1, q.y1
        };
        float uv[12] = {
            q.s0, q.t0, q.s1, q.t1,
            q.s1, q.t0, q.s0, q.t0,
            q.s0, q.t1, q.s1, q.t1
        };
        float c[24] = {
            R, G, B, A, R, G, B, A,
            R, G, B, A, R, G, B, A,
            R, G, B, A, R, G, B, A
        };

        gl::BindVertexArray(m_vao);
        gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[0]);
        gl::BufferData(GL_ARRAY_BUFFER, 12*sizeof(float), v, GL_STATIC_DRAW);
        gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[1]);
        gl::BufferData(GL_ARRAY_BUFFER, 12*sizeof(float), uv, GL_STATIC_DRAW);
        gl::BindBuffer(GL_ARRAY_BUFFER, m_vbos[2]);
        gl::BufferData(GL_ARRAY_BUFFER, 24*sizeof(float), c, GL_STATIC_DRAW);
        gl::DrawArrays(GL_TRIANGLES, 0, 6);
    }
}

}
