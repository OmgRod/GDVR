#include "VRRenderer.hpp"
#include "Shaders.hpp"

void VRRenderer::initialise() {
    m_roomShader = createProgram(CUBE_VERT_SHADER, ROOM_FRAG_SHADER);
    
    // Cube VAO setup (Placeholder for implementation)
    glGenVertexArrays(1, &m_cubeVAO);
    
    // Screen Quad setup
    float quadVertices[] = {
        -1.0f,  1.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f
    };
    glGenVertexArrays(1, &m_screenVAO);
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(m_screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void VRRenderer::renderEye(GLuint gdTexture, const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(m_roomShader);
    glUniformMatrix4fv(glGetUniformLocation(m_roomShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_roomShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glBindVertexArray(m_cubeVAO);
    glBindTexture(GL_TEXTURE_2D, gdTexture);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
}

void VRRenderer::drawToScreen(GLuint gdTexture) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 1920, 1080);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_roomShader);
    glBindVertexArray(m_screenVAO);
    glBindTexture(GL_TEXTURE_2D, gdTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

GLuint VRRenderer::createProgram(const char* vert, const char* frag) {
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vert, NULL);
    glCompileShader(v);
    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &frag, NULL);
    glCompileShader(f);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    return p;
}

void VRRenderer::shutdown() {
    glDeleteProgram(m_roomShader);
    glDeleteVertexArrays(1, &m_cubeVAO);
    glDeleteVertexArrays(1, &m_screenVAO);
}
