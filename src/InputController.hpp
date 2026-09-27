#pragma once

#include <Geode/binding/PlayLayer.hpp>

#include <cstdint>

namespace upf {
    class InputController {
    public:
        void setMask(PlayLayer* playLayer, std::uint8_t mask);
        void releaseAll(PlayLayer* playLayer);
        [[nodiscard]] std::uint8_t currentMask() const { return m_currentMask; }

    private:
        void send(PlayLayer* playLayer, std::uint8_t button, bool down);
        std::uint8_t m_currentMask = 0;
    };
}
