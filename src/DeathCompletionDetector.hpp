#pragma once

#include <Geode/binding/PlayLayer.hpp>

#include <string>

namespace upf {
    class DeathCompletionDetector {
    public:
        [[nodiscard]] bool playerDied(PlayLayer* playLayer) const;
        [[nodiscard]] bool levelCompleted(PlayLayer* playLayer) const;
        [[nodiscard]] float progress(PlayLayer* playLayer) const;
        [[nodiscard]] std::string gameMode(PlayLayer* playLayer) const;
    };
}
