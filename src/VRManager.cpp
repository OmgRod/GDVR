#include "VRManager.hpp"
#include "VRRenderer.hpp"

using namespace geode::prelude;

VRManager& VRManager::get() {
    static VRManager instance;
    return instance;
}

bool VRManager::init() {
    if (m_initialised) return true;

    // Viewport dimensions are usually captured here; ensure valid GL context.
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    m_width = viewport[2];
    m_height = viewport[3];

    if (m_width <= 0 || m_height <= 0) {
        // Log an error but try to continue, maybe default to 1920x1080
        m_width = 1920; m_height = 1080;
    }

    if (!createGDTexture()) return false;
    if (!m_openXR.initialise()) return false;
    m_renderer.initialise();

    m_initialised = true;
    log::info("VR initialised: {}x{}", m_width, m_height);
    return true;
}

void VRManager::update() {
    if (!m_initialised) return;

    captureGDFrame();

    XrTime displayTime;
    if (!m_openXR.waitFrame(&displayTime)) return;
    
    m_openXR.beginFrame();
    
    auto& eyes = m_openXR.getEyes();
    for (const auto& eye : eyes) {
        GLuint image = m_openXR.acquireImage(eye);
        
        // Render
        m_renderer.renderEye(m_gdTexture, eye.viewMatrix, eye.projection);
        
        m_openXR.releaseImage(eye);
    }
    
    m_openXR.submitFrame(eyes);
    
    // Final step: draw to desktop monitor for Steam Link
    m_renderer.drawToScreen(m_gdTexture);
}

void VRManager::captureGDFrame() {
    glBindTexture(GL_TEXTURE_2D, m_gdTexture);
    // Copy the current backbuffer to our texture
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
