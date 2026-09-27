#include "InputController.hpp"
#include "SearchEngine.hpp"

#include <iterator>

namespace upf {
    namespace {
        constexpr std::uint8_t kButtons[] = { kJumpMask, kLeftMask, kRightMask };
        constexpr int kGameButtons[] = {
            static_cast<int>(PlayerButton::Jump),
            static_cast<int>(PlayerButton::Left),
            static_cast<int>(PlayerButton::Right),
        };
    }

    void InputController::setMask(PlayLayer* playLayer, std::uint8_t mask) {
        if (!playLayer || mask > 7) {
            return;
        }
        for (std::size_t i = 0; i < std::size(kButtons); ++i) {
            auto wasDown = (m_currentMask & kButtons[i]) != 0;
            auto shouldBeDown = (mask & kButtons[i]) != 0;
            if (wasDown != shouldBeDown) {
                send(playLayer, kButtons[i], shouldBeDown);
            }
        }
        m_currentMask = mask;
    }

    void InputController::releaseAll(PlayLayer* playLayer) {
        if (playLayer) {
            for (auto button : kButtons) {
                send(playLayer, button, false);
            }
        }
        m_currentMask = 0;
    }

    void InputController::send(PlayLayer* playLayer, std::uint8_t button, bool down) {
        if (!playLayer) {
            return;
        }
        for (std::size_t i = 0; i < std::size(kButtons); ++i) {
            if (kButtons[i] == button) {
                playLayer->handleButton(down, kGameButtons[i], true);
                return;
            }
        }
    }
}
