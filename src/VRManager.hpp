#pragma once

#include <Geode/Geode.hpp>
#include "OpenXR/OpenXRManager.hpp"
#include "VRRenderer.hpp"

class VRManager {
public:
    static VRManager& get();
    bool init();
    void update();
    void shutdown();
    GLuint getGDTexture() const;

private:
    VRManager() = default;
    VRManager(const VRManager&) = delete;
    VRManager& operator=(const VRManager&) = delete;
    VRManager(VRManager&&) = delete;
    VRManager& operator=(VRManager&&) = delete;

    bool createGDTexture();
    void captureGDFrame();

    OpenXRManager m_openXR;
    VRRenderer m_renderer;
    GLuint m_gdTexture = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_initialised = false;
    bool m_initFailed = false;
};