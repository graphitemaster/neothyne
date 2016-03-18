#include <string.h>

#include "r_geom.h"

#include "m_vec.h"
#include "m_half.h"
#include "m_trig.h"

#include "u_vector.h"

namespace r {

geom::geom()
    : vbo(0)
    , ibo(0)
    , vao(0)
{
}

geom::~geom() {
    if (buffers[0])
        gl::DeleteBuffers(buffers[1] ? 2 : 1, buffers);
    if (vao)
        gl::DeleteVertexArrays(1, &vao);
}

void geom::upload(bool index) {
    gl::GenVertexArrays(1, &vao);
    gl::GenBuffers(index ? 2 : 1, buffers);
}

bool quad::upload() {
    geom::upload(false);

    alignas(16) static const GLfloat vertices[] = {
        -1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, -1.0f,
        1.0f, 1.0f
    };

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        const auto convert = m::convertToHalf(vertices, sizeof vertices / sizeof *vertices);
        gl::BufferData(GL_ARRAY_BUFFER, convert.size() * sizeof convert[0], &convert[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 2, GL_HALF_FLOAT, GL_FALSE, sizeof convert[0] * 2, (const GLvoid *)(0)); // position
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof vertices[0] * 2, ATTRIB_OFFSET(0)); // position
    }
    gl::EnableVertexAttribArray(0);

    return true;
}

void quad::render() {
    gl::BindVertexArray(vao);
    gl::DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

sphere::sphere()
    : m_indices(0)
{
}

bool sphere::upload() {
    geom::upload();

    constexpr size_t numVertices = (kStacks + 1) * (kSlices + 1);
    u::vector<m::vec3> vertices(numVertices);

    const float ds = 1.0f / kSlices;
    const float dt = 1.0f / kStacks;
    float t = 1.0f;
    for (size_t i = 0; i < kStacks + 1; i++) {
        float s = 0.0f;
        const float rho = m::kPi * (1.0f - t);
        const float sinrho = i && i < kStacks ? m::sin(rho) : 0.0f;
        const float cosrho = !i ? 1.0f : (i < kStacks ? m::cos(rho) : -1.0f);
        for (size_t j = 0; j < kSlices + 1; j++) {
            float theta = j==kSlices ? 0 : 2*m::kPi*s;
            auto &v = vertices[i*(kSlices+1) + j];
            m::vec2 sc = m::sincos(theta);
            v = m::vec3(sc.x*sinrho, sc.y*sinrho, -cosrho);
            s += ds;
        }
        t -= dt;
    }

    m_indices = (kStacks - 1) * kSlices * 3 * 2;
    u::vector<GLushort> indices(m_indices);
    GLushort *curIndex = &indices[0];
    for (size_t i = 0; i < kStacks; i++) {
        for (size_t k = 0; k < kSlices; k++) {
            const size_t j = i%2 ? kSlices-k-1 : k;
            if (i) {
                *curIndex++ = i*(kSlices+1)+j;
                *curIndex++ = (i+1)*(kSlices+1)+j;
                *curIndex++ = i*(kSlices+1)+j+1;
            }
            if (i + 1 < kStacks) {
                *curIndex++ = i*(kSlices+1)+j+1;
                *curIndex++ = (i+1)*(kSlices+1)+j;
                *curIndex++ = (i+1)*(kSlices+1)+j+1;
            }
        }
    }

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        const auto convert = m::convertToHalf((const float *const)&vertices[0], numVertices * 3);
        gl::BufferData(GL_ARRAY_BUFFER, convert.size() * sizeof convert[0], &convert[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, 0, ATTRIB_OFFSET(0)); // position
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, numVertices * sizeof vertices[0], &vertices[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof vertices[0], ATTRIB_OFFSET(0)); // position
    }
    gl::EnableVertexAttribArray(0);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices * sizeof indices[0], &indices[0], GL_STATIC_DRAW);

    return true;
}

void sphere::render() {
    gl::BindVertexArray(vao);
    gl::DrawElements(GL_TRIANGLES, m_indices, GL_UNSIGNED_SHORT, 0);
}

bool bbox::upload() {
    geom::upload();

    // 1x1x1 cube (centered on origin)
    alignas(16) static const GLfloat vertices[] = {
        -0.5f, -0.5f, -0.5f, 1.0f,
         0.5f, -0.5f, -0.5f, 1.0f,
         0.5f,  0.5f, -0.5f, 1.0f,
        -0.5f,  0.5f, -0.5f, 1.0f,
        -0.5f, -0.5f,  0.5f, 1.0f,
         0.5f, -0.5f,  0.5f, 1.0f,
         0.5f,  0.5f,  0.5f, 1.0f,
        -0.5f,  0.5f,  0.5f, 1.0f
    };

    static const GLubyte indices[] = {
        0, 1, 2, 3,
        4, 5, 6, 7,
        0, 4, 1, 5, 2, 6, 3, 7
    };

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        const auto convert = m::convertToHalf(vertices, sizeof vertices / sizeof *vertices);
        gl::BufferData(GL_ARRAY_BUFFER, convert.size() * sizeof convert[0], &convert[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 4, GL_HALF_FLOAT, GL_FALSE, 0, ATTRIB_OFFSET(0)); // position
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, ATTRIB_OFFSET(0)); // position
    }
    gl::EnableVertexAttribArray(0);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);

    return true;
}

void bbox::render() {
    gl::BindVertexArray(vao);
    gl::DrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_BYTE, 0);
    gl::DrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_BYTE, (void*)4);
    gl::DrawElements(GL_LINES, 8, GL_UNSIGNED_BYTE, (void*)8);
}

bool cone::upload() {
    geom::upload();
    u::vector<m::vec3> vertices;

    for (float a = 0.0f; a < m::kTau; a += m::kPi / (kSlices+1) ) {
        const m::vec2 sc = m::sincos(a);
        vertices.push_back({sc.x, 1.0f, sc.y});
    }

    u::vector<GLushort> indices;
    const size_t count = vertices.size();
    for (size_t i = 0; i < count; i++)
        indices.push_back(i);

    m_indices.first = indices.size();
    vertices.push_back(m::vec3::origin);
    indices.push_back(m_indices.first);
    for (size_t i = 0; i < count; i++)
        indices.push_back(i);
    m_indices.second = indices.size() - m_indices.first;

    gl::BindVertexArray(vao);
    gl::BindBuffer(GL_ARRAY_BUFFER, vbo);

    if (gl::has(gl::ARB_half_float_vertex)) {
        const auto convert = m::convertToHalf((const float *)&vertices[0], vertices.size() * 3);
        gl::BufferData(GL_ARRAY_BUFFER, convert.size() * sizeof convert[0], &convert[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_HALF_FLOAT, GL_FALSE, 0, ATTRIB_OFFSET(0)); // position
    } else {
        gl::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof vertices[0], &vertices[0], GL_STATIC_DRAW);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, ATTRIB_OFFSET(0)); // position
    }
    gl::EnableVertexAttribArray(0);

    gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl::BufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof indices[0], &indices[0], GL_STATIC_DRAW);

    return true;
}

void cone::render(bool bottom) {
    gl::BindVertexArray(vao);
    if (bottom)
        gl::DrawElements(GL_TRIANGLE_FAN, m_indices.first, GL_UNSIGNED_SHORT, 0);
    gl::DrawElements(GL_TRIANGLE_FAN, m_indices.second, GL_UNSIGNED_SHORT,
        (const GLvoid *)(m_indices.first * sizeof(GLushort)));
}

}
