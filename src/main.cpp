#include "PathfinderController.hpp"
#include "PathfinderPopup.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(UniversalPathfinderPlayLayer, PlayLayer) {
    void onEnterTransitionDidFinish() {
        PlayLayer::onEnterTransitionDidFinish();
        upf::PathfinderController::get().onLevelReady(this);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        upf::PathfinderController::get().onFrame(this);
    }

    void onExit() {
        upf::PathfinderController::get().onLayerExit(this);
        upf::resetPathfinderUI();
        PlayLayer::onExit();
    }
};

class $modify(UniversalPathfinderGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto playLayer = PlayLayer::get();
        if (playLayer == this && upf::PathfinderController::get().shouldFreeze(playLayer)) {
            return;
        }
        GJBaseGameLayer::update(dt);
    }
};

$on_mod(Loaded) {
    log::info("Universal Pathfinder loaded for Geometry Dash 2.2081");
    listenForKeybindSettingPresses("open-menu", [](Keybind const&, bool down, bool repeat, double) {
        if (down && !repeat) {
            upf::openPathfinderMenu();
        }
    });
}
