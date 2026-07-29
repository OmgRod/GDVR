#pragma once
#include <Geode/Geode.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

enum class Eye { Left, Right };

class VRRenderer {
public:
    void initialise();
    void renderEye(GLuint gdTexture, const glm::mat4& view, const glm::mat4& proj);
    void drawToScreen(GLuint gdTexture);
    void shutdown();
private:
    GLuint m_roomShader;
    GLuint m_cubeVAO;
    GLuint m_screenVAO;
    GLuint createProgram(const char* vertSource, const char* fragSource);
};
