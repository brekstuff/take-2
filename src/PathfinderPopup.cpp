#include "PathfinderPopup.hpp"
#include "PathfinderController.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <string>
#include <utility>

using namespace geode::prelude;

namespace upf {
    namespace {
        bool g_menuPopupOpen = false;
        bool g_progressPopupOpen = false;
        bool g_auxPopupOpen = false;

        void addButton(CCMenu* menu, CCNode* target, char const* title, SEL_MenuHandler callback, float y, float scale = 0.7f) {
            auto sprite = ButtonSprite::create(title, "bigFont.fnt", "GJ_button_01.png", 0.8f);
            if (!sprite) return;
            sprite->setScale(scale);
            auto item = CCMenuItemSpriteExtra::create(sprite, target, callback);
            menu->addChildAtPosition(item, Anchor::Center, ccp(0.f, y));
        }

        CCLabelBMFont* addLabel(CCNode* parent, char const* text, float y, float scale) {
            auto label = CCLabelBMFont::create(text, "bigFont.fnt");
            if (!label) return nullptr;
            label->setScale(scale);
            parent->addChildAtPosition(label, Anchor::Center, ccp(0.f, y));
            return label;
        }

        void showMessage(char const* title, std::string const& message) {
            FLAlertLayer::create(title, message.c_str(), "OK")->show();
        }
    }

    PathfinderMenuPopup* PathfinderMenuPopup::create() {
        auto ret = new PathfinderMenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PathfinderMenuPopup::init() {
        if (!Popup::init(310.f, 220.f)) return false;
        this->setTitle("Universal Pathfinder");
        m_noElasticity = true;
        addButton(m_buttonMenu, this, "PATHFIND", menu_selector(PathfinderMenuPopup::onPathfind), 22.f, 0.78f);
        addButton(m_buttonMenu, this, "LOAD", menu_selector(PathfinderMenuPopup::onLoad), -42.f, 0.78f);
        return true;
    }

    void openPathfinderMenu() {
        if (g_menuPopupOpen || g_progressPopupOpen || g_auxPopupOpen || PathfinderController::get().snapshot().active || !PlayLayer::get()) return;
        if (auto popup = PathfinderMenuPopup::create()) {
            g_menuPopupOpen = true;
            popup->show();
        }
    }

    void resetPathfinderUI() {
        g_menuPopupOpen = false;
        g_progressPopupOpen = false;
        g_auxPopupOpen = false;
    }

    void PathfinderMenuPopup::onClose(CCObject* sender) {
        g_menuPopupOpen = false;
        Popup::onClose(sender);
    }

    void PathfinderMenuPopup::onPathfind(CCObject*) {
        auto playLayer = PlayLayer::get();
        PathfinderController::get().start(playLayer);
        auto state = PathfinderController::get().snapshot();
        this->onClose(nullptr);
        if (state.active) {
            if (auto progress = PathfinderProgressPopup::create()) progress->show();
            else {
                PathfinderController::get().cancelSearch(playLayer);
                showMessage("Universal Pathfinder", "Couldn't open the progress screen.");
            }
        }
        else {
            auto message = playLayer && playLayer->m_level
                ? fmt::format("Couldn't start pathfinding: {}.", state.status)
                : std::string("Open a level before starting a pathfind.");
            showMessage("Universal Pathfinder", message);
        }
    }

    void PathfinderMenuPopup::onLoad(CCObject*) {
        this->onClose(nullptr);
        if (auto popup = PathfinderRoutesPopup::create()) popup->show();
    }

    PathfinderProgressPopup* PathfinderProgressPopup::create() {
        auto ret = new PathfinderProgressPopup();
        if (ret->init()) {
            g_progressPopupOpen = true;
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PathfinderProgressPopup::init() {
        auto winSize = CCDirector::get()->getWinSize();
        if (!Popup::init(winSize, "GJ_square01.png")) return false;
        this->setColor(ccc3(0, 0, 0));
        this->setOpacity(255);
        if (m_bgSprite) m_bgSprite->setVisible(false);
        if (m_closeBtn) m_closeBtn->setVisible(false);
        m_noElasticity = true;

        m_statusLabel = addLabel(m_mainLayer, "Starting pathfind...", 45.f, 0.75f);
        m_progressLabel = addLabel(m_mainLayer, "0.00%", -5.f, 1.3f);
        addButton(m_buttonMenu, this, "CANCEL", menu_selector(PathfinderProgressPopup::onCancel), -100.f, 0.8f);
        this->schedule(schedule_selector(PathfinderProgressPopup::refresh), 0.15f);
        this->refresh(0.f);
        return true;
    }

    void PathfinderProgressPopup::refresh(float) {
        auto state = PathfinderController::get().snapshot();
        if (m_statusLabel) m_statusLabel->setString(state.status.c_str());
        if (m_progressLabel) m_progressLabel->setString(fmt::format("{:.2f}%", state.progress).c_str());

        if (!m_openedNamePrompt && state.status == "Solution found" && state.routeAvailable) {
            m_openedNamePrompt = true;
            g_progressPopupOpen = false;
            this->unschedule(schedule_selector(PathfinderProgressPopup::refresh));
            Popup::onClose(nullptr);
            if (auto popup = PathfinderSavePopup::create()) popup->show();
        }
    }

    void PathfinderProgressPopup::onCancel(CCObject*) {
        this->onClose(nullptr);
    }

    void PathfinderProgressPopup::onClose(CCObject* sender) {
        g_progressPopupOpen = false;
        PathfinderController::get().cancelSearch(PlayLayer::get());
        Popup::onClose(sender);
    }

    void PathfinderProgressPopup::keyBackClicked() {
        this->onCancel(nullptr);
    }

    PathfinderSavePopup* PathfinderSavePopup::create() {
        auto ret = new PathfinderSavePopup();
        if (ret->init()) {
            g_auxPopupOpen = true;
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PathfinderSavePopup::init() {
        if (!Popup::init(360.f, 220.f)) return false;
        if (m_closeBtn) m_closeBtn->setVisible(false);
        this->setTitle("Name this pathfind");
        m_noElasticity = true;

        m_nameInput = TextInput::create(245.f, "Pathfind name");
        if (!m_nameInput) return false;
        m_nameInput->setCommonFilter(CommonFilter::Name);
        m_nameInput->setMaxCharCount(64);
        m_mainLayer->addChildAtPosition(m_nameInput, Anchor::Center, ccp(0.f, 20.f));

        addButton(m_buttonMenu, this, "SAVE", menu_selector(PathfinderSavePopup::onSave), -52.f, 0.65f);
        addButton(m_buttonMenu, this, "DISCARD", menu_selector(PathfinderSavePopup::onDiscard), -102.f, 0.65f);
        return true;
    }

    void PathfinderSavePopup::onClose(CCObject* sender) {
        g_auxPopupOpen = false;
        Popup::onClose(sender);
    }

    void PathfinderSavePopup::keyBackClicked() {}

    void PathfinderSavePopup::onSave(CCObject*) {
        auto input = m_nameInput->getString();
        auto name = std::string(input.c_str());
        if (!PathfinderController::get().saveRouteAs(std::move(name))) {
            showMessage("Couldn't save pathfind", "Enter a name for the route and try again.");
            return;
        }
        this->onClose(nullptr);
    }

    void PathfinderSavePopup::onDiscard(CCObject*) {
        PathfinderController::get().stop(PlayLayer::get());
        this->onClose(nullptr);
    }

    PathfinderRoutesPopup* PathfinderRoutesPopup::create() {
        auto ret = new PathfinderRoutesPopup();
        if (ret->init()) {
            g_auxPopupOpen = true;
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool PathfinderRoutesPopup::init() {
        if (!Popup::init(470.f, 360.f)) return false;
        this->setTitle("Saved pathfinds");
        m_noElasticity = true;
        m_routes = PathfinderController::get().savedRoutes();

        if (m_routes.empty()) {
            addLabel(m_mainLayer, "No saved pathfinds yet", 0.f, 0.55f);
            return true;
        }

        constexpr float width = 410.f;
        constexpr float viewHeight = 255.f;
        constexpr float rowHeight = 42.f;
        auto list = ScrollLayer::create(CCSize { width, viewHeight });
        if (!list) return false;
        auto contentHeight = std::max(viewHeight, rowHeight * static_cast<float>(m_routes.size()));
        list->setContentLayerSize(CCSize { width, contentHeight });
        auto menu = CCMenu::create();
        menu->setPosition(ccp(width / 2.f, contentHeight / 2.f));
        list->m_contentLayer->addChild(menu);

        for (std::size_t i = 0; i < m_routes.size(); ++i) {
            auto const& route = m_routes[i];
            auto caption = fmt::format("{}  |  Level {}", route.name, route.levelID);
            auto sprite = ButtonSprite::create(caption.c_str(), 390, 370, 0.48f, true, "bigFont.fnt", "GJ_button_01.png", 34.f);
            if (!sprite) continue;
            auto item = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(PathfinderRoutesPopup::onSelectRoute));
            item->setTag(static_cast<int>(i));
            auto y = contentHeight / 2.f - rowHeight * (static_cast<float>(i) + 0.5f);
            item->setPosition(ccp(0.f, y));
            menu->addChild(item);
        }
        list->scrollToTop();
        m_mainLayer->addChildAtPosition(list, Anchor::Center, ccp(0.f, -18.f));
        return true;
    }

    void PathfinderRoutesPopup::onSelectRoute(CCObject* sender) {
        auto item = static_cast<CCNode*>(sender);
        auto index = static_cast<std::size_t>(item->getTag());
        if (index >= m_routes.size()) return;
        auto const& route = m_routes[index];
        if (!PathfinderController::get().loadNamedRoute(route.file, PlayLayer::get())) {
            showMessage("Pathfind not loaded", fmt::format("Open level {} before loading '{}'.", route.levelID, route.name));
            return;
        }
        this->onClose(nullptr);
    }

    void PathfinderRoutesPopup::onClose(CCObject* sender) {
        g_auxPopupOpen = false;
        Popup::onClose(sender);
    }
}
