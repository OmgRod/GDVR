#include <Geode/Geode.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "VRManager.hpp"

using namespace geode::prelude;

class $modify(MyEGLView, CCEGLView) {
    void swapBuffers() {
        // Only run the OpenXR render loop after the user has explicitly
        // enabled VR via the button. This avoids initialising OpenXR before
        // the Android activity lifecycle is ready.
        if (VRManager::get().isEnabled()) {
            VRManager::get().update();
        }

        // Always call the original so the flat Cocos2d view keeps rendering.
        CCEGLView::swapBuffers();
    }
};

class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto menu = CCMenu::create();
        menu->setID("gdvr-menu");

        auto btnSprite = ButtonSprite::create("VR", "bigFont.fnt", "GJ_button_01.png");
        btnSprite->setScale(0.6f);
        
        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite,
            this,
            menu_selector(MyMenuLayer::onToggleVR)
        );
        btn->setID("toggle-vr-button");
        menu->addChild(btn);
        
        // Position at the bottom center, slightly above the edge
        auto winSize = CCDirector::get()->getWinSize();
        menu->setPosition({ winSize.width / 2.f, 25.f });
        
        this->addChild(menu);

        return true;
    }

    void onToggleVR(CCObject* sender) {
        log::info("User clicked the Toggle VR button in MenuLayer!");
#ifdef GEODE_IS_ANDROID
        // We are already inside the correct activity, so there is no need to
        // launch an Intent. Just tell VRManager to initialise OpenXR and start
        // the render loop. The manifest category handles OpenXR runtime routing.
        VRManager::get().startVR();
#else
        log::info("Toggle VR is only supported on Android (Meta Quest).");
#endif
    }
};