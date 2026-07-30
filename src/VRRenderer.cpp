#include "VRRenderer.hpp"
#include "Shaders.hpp"

void VRRenderer::initialise() {
    m_roomShader = createProgram(CUBE_VERT_SHADER, ROOM_FRAG_SHADER);
    
    // Screen Quad setup (16:9 aspect ratio, 2 meters wide, 2 meters in front of player)
    float quadVertices[] = {
        // positions (x, y, z)    // texCoords (u, v)
        -0.8f,  0.45f, -2.0f,    0.0f, 1.0f,
        -0.8f, -0.45f, -2.0f,    0.0f, 0.0f,
         0.8f, -0.45f, -2.0f,    1.0f, 0.0f,
        -0.8f,  0.45f, -2.0f,    0.0f, 1.0f,
         0.8f, -0.45f, -2.0f,    1.0f, 0.0f,
         0.8f,  0.45f, -2.0f,    1.0f, 1.0f
    };
    glGenVertexArrays(1, &m_screenVAO);
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindVertexArray(m_screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    
    // Generate FBO for rendering to swapchain
    glGenFramebuffers(1, &m_fbo);
}

void VRRenderer::renderEye(GLuint swapchainImage, GLuint gdTexture, const glm::mat4& view, const glm::mat4& proj) {
    // Bind the swapchain image to our FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, swapchainImage, 0);
    
    // Hardcoded viewport size based on our swapchain creation (2048x2048)
    glViewport(0, 0, 2048, 2048);
    
    // Clear the background (the "void")
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_roomShader);
    glUniformMatrix4fv(glGetUniformLocation(m_roomShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(m_roomShader, "projection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(m_roomShader, "model"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    
    glBindVertexArray(m_screenVAO);
    glBindTexture(GL_TEXTURE_2D, gdTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    // Unbind FBO
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteVertexArrays(1, &m_screenVAO);
}
