#include "r_quad.h"

namespace r {

quad::~quad(void) {
    gl::DeleteBuffers(2, m_buffers);
}

bool quad::upload(void) {
    gl::GenVertexArrays(1, &m_vao);
    gl::BindVertexArray(m_vao);

    gl::GenBuffers(2, m_buffers);
    gl::BindBuffer(GL_ARRAY_BUFFER, m_vbo);

    static const GLfloat vertices[] = {
        -1.0f,-1.0f, 0.0f, 0.0f,  0.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, -1.0f,
         1.0f, 1.0f, 0.0f, 1.0f, -1.0f,
         1.0f,-1.0f, 0.0f, 1.0f,  0.0f,
    };

    static const GLubyte indices[] = { 0, 1, 2, 0, 2, 3 };

    gl::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, GL_OFFSET(0)); // position
    gl::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat)*5, GL_OFFSET(3)); // uvs
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

}
