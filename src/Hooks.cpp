#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "VRManager.hpp"

using namespace geode::prelude;

class $modify(MyEGLView, CCEGLView) {
    void swapBuffers() {
        static bool firstFrame = true;
        if (firstFrame) {
            log::info("VRManager: First frame swapBuffers called");
            firstFrame = false;
        }

        // Initialize VRManager lazily on the first frame
        VRManager::get().init();

        // Run VR rendering before swapping buffers
        VRManager::get().update();
        
        // Call the original swapBuffers to show the flat mirror
        CCEGLView::swapBuffers();
    }
};