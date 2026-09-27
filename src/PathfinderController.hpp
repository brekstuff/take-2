#pragma once

#include "DeathCompletionDetector.hpp"
#include "GameStateManager.hpp"
#include "InputController.hpp"
#include "RouteRecorder.hpp"
#include "SearchEngine.hpp"

#include <Geode/binding/PlayLayer.hpp>

#include <cstdint>
#include <string>

namespace upf {
    struct ControllerSnapshot {
        std::string status;
        std::string gameMode;
        float progress = 0.f;
        float bestProgress = 0.f;
        int attempts = 0;
        int maxAttempts = 0;
        int frameResolution = 0;
        int currentDepth = 0;
        int maxDepth = 0;
        std::uint64_t searchStates = 0;
        std::uint64_t estimatedStates = 0;
        bool paused = false;
        bool active = false;
        bool routeAvailable = false;
    };

    class PathfinderController {
    public:
        static PathfinderController& get();

        void onLevelReady(PlayLayer* playLayer);
        void onFrame(PlayLayer* playLayer);
        void onLayerExit(PlayLayer* playLayer);
        void start(PlayLayer* playLayer);
        void cancelSearch(PlayLayer* playLayer);
        void pauseOrResume();
        void stop(PlayLayer* playLayer);
        void reset(PlayLayer* playLayer);
        bool saveRouteAs(std::string name);
        bool loadNamedRoute(std::filesystem::path const& path, PlayLayer* playLayer);
        [[nodiscard]] std::vector<SavedRouteInfo> savedRoutes() const;
        [[nodiscard]] ControllerSnapshot snapshot() const;
        [[nodiscard]] bool shouldFreeze(PlayLayer* playLayer) const;

    private:
        enum class Phase {
            Stopped,
            WaitingToStart,
            Searching,
            WaitingAfterFailure,
            Replaying,
            Paused,
            Success,
            Replayed,
            Exhausted,
            AttemptLimit,
            ReplayFailed,
            ResetFailed,
            RouteCaptureFailed,
        };

        PathfinderController() = default;
        SearchOptions readSearchOptions() const;
        int currentLevelID(PlayLayer* playLayer) const;
        void beginRestart(PlayLayer* playLayer, bool replay);
        void beginAttempt(PlayLayer* playLayer);
        void failBranch(PlayLayer* playLayer, bool waitForDeathAnimation);
        void finishSearch(PlayLayer* playLayer);
        void handleReplayFrame(PlayLayer* playLayer);
        void handleSearchFrame(PlayLayer* playLayer);
        void updateCurrentProgress(PlayLayer* playLayer);
        void setStatus(Phase phase);
        void saveSearchIfEnabled();

        SearchEngine m_search;
        GameStateManager m_gameState;
        InputController m_input;
        DeathCompletionDetector m_detector;
        RouteRecorder m_route;
        SearchOptions m_options;
        Phase m_phase = Phase::Stopped;
        Phase m_resumePhase = Phase::Stopped;
        int m_levelID = 0;
        int m_routeLevelID = 0;
        int m_deathDelayRemaining = 0;
        std::uint64_t m_frame = 0;
        int m_segmentRemaining = 0;
        std::size_t m_cursorDepth = 0;
        float m_currentProgress = 0.f;
        std::string m_currentMode = "Unknown";
        bool m_pendingReplay = false;
        bool m_saveSearch = true;
        bool m_routeHasLevel = false;
    };
}
