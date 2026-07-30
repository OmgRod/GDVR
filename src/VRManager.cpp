#include "VRManager.hpp"
#include "VRRenderer.hpp"

using namespace geode::prelude;

VRManager& VRManager::get() {
    static VRManager instance;
    return instance;
}

bool VRManager::init() {
    if (m_initialised) return true;
    if (m_initFailed) return false;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    m_width = viewport[2];
    m_height = viewport[3];

    if (m_width <= 0 || m_height <= 0) {
        m_width = 1920; m_height = 1080;
    }

    log::info("VRManager: Creating GD texture...");
    if (!createGDTexture()) {
        log::error("VRManager: Failed to create GD texture");
        return false;
    }
    
    log::info("VRManager: Initializing OpenXR...");
    if (!m_openXR.initialise()) {
        log::error("VRManager: OpenXR initialization failed");
        m_initFailed = true;
        return false;
    }
    
    log::info("VRManager: Initializing renderer...");
    m_renderer.initialise();

    m_initialised = true;
    log::info("VR initialised: {}x{}", m_width, m_height);
    return true;
}

void VRManager::update() {
    if (!m_initialised) return;

    // Process OpenXR event state machine transitions (IDLE -> READY -> RUNNING)
    m_openXR.pollEvents();

    captureGDFrame();

    XrTime displayTime;
    bool shouldRender = false;
    if (!m_openXR.waitFrame(&shouldRender, &displayTime)) return;
    
    m_openXR.beginFrame();
    
    if (shouldRender) {
        // Retrieve eye tracking data from reference space
        m_openXR.locateViews();
        
        auto& eyes = m_openXR.getEyes();
        for (const auto& eye : eyes) {
            GLuint image = m_openXR.acquireImage(eye);
            
            m_renderer.renderEye(image, m_gdTexture, eye.viewMatrix, eye.projection);
            
            m_openXR.releaseImage(eye);
        }
        
        m_openXR.submitFrame(eyes);
    } else {
        m_openXR.submitEmptyFrame();
    }
}

void VRManager::captureGDFrame() {
    glBindTexture(GL_TEXTURE_2D, m_gdTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, m_width, m_height);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool VRManager::createGDTexture() {
    glGenTextures(1, &m_gdTexture);
    glBindTexture(GL_TEXTURE_2D, m_gdTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

GLuint VRManager::getGDTexture() const { return m_gdTexture; }

void VRManager::shutdown() {
    m_openXR.shutdown();
    m_renderer.shutdown();
    if (m_gdTexture) glDeleteTextures(1, &m_gdTexture);
    m_initialised = false;
}
