#pragma once

#include "RouteRecorder.hpp"

#include <Geode/ui/Popup.hpp>

#include <vector>

namespace cocos2d {
    class CCLabelBMFont;
    class CCObject;
}

namespace geode {
    class TextInput;
    class ScrollLayer;
}

namespace upf {
    void openPathfinderMenu();
    void resetPathfinderUI();

    class PathfinderMenuPopup : public geode::Popup {
    public:
        static PathfinderMenuPopup* create();

    protected:
        bool init() override;
        void onClose(cocos2d::CCObject*) override;

    private:
        void onPathfind(cocos2d::CCObject*);
        void onLoad(cocos2d::CCObject*);
    };

    class PathfinderProgressPopup : public geode::Popup {
    public:
        static PathfinderProgressPopup* create();

    protected:
        bool init() override;
        void onClose(cocos2d::CCObject*) override;
        void keyBackClicked() override;

    private:
        void refresh(float dt);
        void onCancel(cocos2d::CCObject*);
        cocos2d::CCLabelBMFont* m_progressLabel = nullptr;
        cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
        bool m_openedNamePrompt = false;
    };

    class PathfinderSavePopup : public geode::Popup {
    public:
        static PathfinderSavePopup* create();

    protected:
        bool init() override;
        void onClose(cocos2d::CCObject*) override;
        void keyBackClicked() override;

    private:
        void onSave(cocos2d::CCObject*);
        void onDiscard(cocos2d::CCObject*);
        geode::TextInput* m_nameInput = nullptr;
    };

    class PathfinderRoutesPopup : public geode::Popup {
    public:
        static PathfinderRoutesPopup* create();

    protected:
        bool init() override;

    private:
        void onSelectRoute(cocos2d::CCObject*);
        void onClose(cocos2d::CCObject*) override;
        std::vector<SavedRouteInfo> m_routes;
    };
}
