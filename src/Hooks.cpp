#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "VRManager.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    VRManager::get().init();
}

class $modify(MyEGLView, CCEGLView) {
    void swapBuffers() {
        // Run VR rendering before swapping buffers
        VRManager::get().update();
        
        // Call the original swapBuffers to show the flat mirror
        CCEGLView::swapBuffers();
    }
};