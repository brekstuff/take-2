#pragma once

#include <Geode/binding/PlayLayer.hpp>

namespace upf {
    enum class FreshStartResult {
        Waiting,
        Ready,
        Failed,
    };

    class GameStateManager {
    public:
        bool restartFromBeginning(PlayLayer* playLayer);
        FreshStartResult waitForFreshStart(PlayLayer* playLayer, bool playerDead);
        void clear();
        [[nodiscard]] bool isWaiting() const { return m_waiting; }

    private:
        bool m_waiting = false;
        int m_waitTicks = 0;
    };
}
