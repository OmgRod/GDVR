#pragma once
#include "OpenXRPlatform.hpp"
#include <vector>
#include <glm/glm.hpp>

struct EyeData {
    GLuint fbo;
    XrSwapchain swapchain;
    std::vector<GLuint> images;
    XrView view;
    glm::mat4 projection;
    glm::mat4 viewMatrix;
};

class OpenXRManager {
public:
    bool initialise();
    void shutdown();
    void pollEvents();
    bool waitFrame(XrTime* displayTime);
    void beginFrame();
    GLuint acquireImage(const EyeData& eye);
    void releaseImage(const EyeData& eye);
    void submitFrame(const std::vector<EyeData>& eyes);
    const std::vector<EyeData>& getEyes() const { return m_eyes; }
    bool isRunning() const { return m_running; }

private:
    bool createSession();
    bool createSwapchain();

    XrInstance m_instance = XR_NULL_HANDLE;
    XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
    XrSession m_session = XR_NULL_HANDLE;
    std::vector<EyeData> m_eyes;
    XrTime m_predictedDisplayTime = 0;
    bool m_running = false;
};
