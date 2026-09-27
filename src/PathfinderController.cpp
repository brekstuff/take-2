#include "PathfinderController.hpp"
#include "Storage.hpp"

#include <Geode/loader/Mod.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace upf {
    PathfinderController& PathfinderController::get() {
        static PathfinderController controller;
        return controller;
    }

    SearchOptions PathfinderController::readSearchOptions() const {
        auto mod = geode::Mod::get();
        SearchOptions options;
        options.frameResolution = static_cast<int>(mod->getSettingValue<std::int64_t>("frame-resolution"));
        options.maxDepth = static_cast<int>(mod->getSettingValue<std::int64_t>("max-depth"));
        options.maxAttempts = static_cast<int>(mod->getSettingValue<std::int64_t>("max-attempts"));
        options.aggressiveness = static_cast<int>(mod->getSettingValue<std::int64_t>("aggressiveness"));
        return options;
    }

    int PathfinderController::currentLevelID(PlayLayer* playLayer) const {
        return playLayer && playLayer->m_level ? static_cast<int>(playLayer->m_level->m_levelID) : 0;
    }

    void PathfinderController::onLevelReady(PlayLayer* playLayer) {
        if (!playLayer) {
            return;
        }
        auto levelID = currentLevelID(playLayer);
        if (m_levelID != levelID && m_phase != Phase::Stopped) {
            stop(playLayer);
        }
        if (m_routeHasLevel && m_routeLevelID != levelID) {
            m_route.clear();
            m_routeHasLevel = false;
        }
        m_levelID = levelID;
        m_currentMode = m_detector.gameMode(playLayer);
    }

    void PathfinderController::start(PlayLayer* playLayer) {
        if (!playLayer || !playLayer->m_level) {
            geode::log::warn("Pathfinder Start was ignored because no level is active");
            return;
        }
        if (m_phase == Phase::Searching || m_phase == Phase::WaitingToStart ||
            m_phase == Phase::WaitingAfterFailure || m_phase == Phase::Replaying || m_phase == Phase::Paused) {
            return;
        }

        m_levelID = currentLevelID(playLayer);
        m_options = readSearchOptions();
        m_saveSearch = geode::Mod::get()->getSettingValue<bool>("save-search");
        bool resumed = m_saveSearch && m_search.load(m_levelID, m_options);
        if (!resumed) {
            m_search.reset(m_options);
        }
        if (m_search.atAttemptLimit()) {
            m_search.reset(m_options);
        }

        m_pendingReplay = false;
        m_route.clear();
        m_routeHasLevel = false;
        m_frame = 0;
        m_cursorDepth = 0;
        m_segmentRemaining = 0;
        m_currentProgress = 0.f;
        m_currentMode = m_detector.gameMode(playLayer);
        m_input.releaseAll(playLayer);
        beginRestart(playLayer, false);
        geode::log::info("Starting Universal Pathfinder on level {}{}", m_levelID, resumed ? " (resuming saved frontier)" : "");
    }

    void PathfinderController::beginRestart(PlayLayer* playLayer, bool replay) {
        m_pendingReplay = replay;
        m_frame = 0;
        m_cursorDepth = 0;
        m_segmentRemaining = 0;
        m_deathDelayRemaining = 0;
        setStatus(Phase::WaitingToStart);
        if (!m_gameState.restartFromBeginning(playLayer)) {
            setStatus(Phase::ResetFailed);
        }
    }

    void PathfinderController::beginAttempt(PlayLayer* playLayer) {
        m_frame = 0;
        m_cursorDepth = 0;
        m_segmentRemaining = 0;
        m_currentProgress = 0.f;
        m_currentMode = m_detector.gameMode(playLayer);
        m_input.releaseAll(playLayer);
        if (m_pendingReplay) {
            m_route.beginReplay();
            setStatus(Phase::Replaying);
            m_pendingReplay = false;
        }
        else {
            // Record only this candidate. Previous failed attempts must never leak into a solved route.
            m_route.beginRecording();
            m_routeLevelID = m_levelID;
            m_routeHasLevel = true;
            m_search.noteAttemptStarted();
            setStatus(Phase::Searching);
            saveSearchIfEnabled();
        }
    }

    void PathfinderController::onFrame(PlayLayer* playLayer) {
        if (!playLayer || playLayer != PlayLayer::get() || m_phase == Phase::Stopped || m_phase == Phase::Paused) {
            return;
        }
        if (playLayer->m_isPaused) {
            return;
        }

        m_currentMode = m_detector.gameMode(playLayer);
        updateCurrentProgress(playLayer);

        if (m_phase == Phase::WaitingToStart) {
            auto result = m_gameState.waitForFreshStart(playLayer, m_detector.playerDied(playLayer));
            if (result == FreshStartResult::Failed) {
                m_input.releaseAll(playLayer);
                m_gameState.clear();
                setStatus(Phase::ResetFailed);
            }
            else if (result == FreshStartResult::Ready) {
                beginAttempt(playLayer);
                if (m_phase == Phase::Searching) {
                    handleSearchFrame(playLayer);
                }
                else if (m_phase == Phase::Replaying) {
                    handleReplayFrame(playLayer);
                }
            }
            return;
        }
        if (m_phase == Phase::WaitingAfterFailure) {
            if (m_deathDelayRemaining > 0) {
                --m_deathDelayRemaining;
                return;
            }
            beginRestart(playLayer, false);
            return;
        }
        if (m_phase == Phase::Replaying) {
            if (m_detector.levelCompleted(playLayer)) {
                m_input.releaseAll(playLayer);
                setStatus(Phase::Replayed);
                return;
            }
            if (m_detector.playerDied(playLayer)) {
                m_input.releaseAll(playLayer);
                setStatus(Phase::ReplayFailed);
                return;
            }
            handleReplayFrame(playLayer);
            return;
        }
        if (m_phase == Phase::Searching) {
            if (m_detector.levelCompleted(playLayer)) {
                finishSearch(playLayer);
                return;
            }
            if (m_detector.playerDied(playLayer)) {
                failBranch(playLayer, true);
                return;
            }
            handleSearchFrame(playLayer);
        }
    }

    void PathfinderController::handleSearchFrame(PlayLayer* playLayer) {
        if (m_segmentRemaining <= 0) {
            if (m_cursorDepth >= static_cast<std::size_t>(m_search.maxDepth()) ||
                !m_search.ensureDecision(m_cursorDepth, playLayer->m_player1)) {
                failBranch(playLayer, false);
                return;
            }

            auto mask = m_search.actionAt(m_cursorDepth);
            m_input.setMask(playLayer, mask);
            m_route.recordInput(m_frame, mask);
            ++m_cursorDepth;
            m_segmentRemaining = m_search.frameResolution();
        }

        --m_segmentRemaining;
        ++m_frame;
    }

    void PathfinderController::handleReplayFrame(PlayLayer* playLayer) {
        auto const& events = m_route.events();
        auto cursor = m_route.replayCursor();
        while (cursor < events.size() && events[cursor].frame <= m_frame) {
            m_input.setMask(playLayer, events[cursor].mask);
            ++cursor;
        }
        m_route.setReplayCursor(cursor);
        ++m_frame;
        if (m_frame > m_route.totalFrames() + 1 && !m_detector.levelCompleted(playLayer)) {
            m_input.releaseAll(playLayer);
            setStatus(Phase::ReplayFailed);
        }
    }

    void PathfinderController::failBranch(PlayLayer* playLayer, bool waitForDeathAnimation) {
        m_search.updateProgress(m_currentProgress);
        m_input.releaseAll(playLayer);
        if (m_search.atAttemptLimit()) {
            setStatus(Phase::AttemptLimit);
            saveSearchIfEnabled();
            return;
        }

        auto result = m_search.advanceAfterFailure();
        if (result == BacktrackResult::Exhausted) {
            m_search.eraseSavedState(m_levelID);
            setStatus(Phase::Exhausted);
            return;
        }
        if (m_search.atAttemptLimit()) {
            setStatus(Phase::AttemptLimit);
            saveSearchIfEnabled();
            return;
        }

        m_cursorDepth = 0;
        m_segmentRemaining = 0;
        m_frame = 0;
        saveSearchIfEnabled();
        if (waitForDeathAnimation) {
            auto speed = std::clamp(static_cast<int>(geode::Mod::get()->getSettingValue<std::int64_t>("search-speed")), 1, 10);
            m_deathDelayRemaining = 11 - speed;
            setStatus(Phase::WaitingAfterFailure);
        }
        else {
            beginRestart(playLayer, false);
        }
    }

    void PathfinderController::finishSearch(PlayLayer* playLayer) {
        m_search.updateProgress(100.f);
        m_input.releaseAll(playLayer);
        if (!m_route.finishRecording(m_frame)) {
            setStatus(Phase::RouteCaptureFailed);
            return;
        }
        m_routeLevelID = m_levelID;
        m_routeHasLevel = true;
        m_search.eraseSavedState(m_levelID);
        setStatus(Phase::Success);
        geode::log::info("Universal Pathfinder found a route for level {}; waiting for a name", m_levelID);
    }

    void PathfinderController::updateCurrentProgress(PlayLayer* playLayer) {
        m_currentProgress = m_detector.progress(playLayer);
        if (m_phase == Phase::Searching) {
            m_search.updateProgress(m_currentProgress);
        }
    }

    void PathfinderController::pauseOrResume() {
        if (m_phase == Phase::Paused) {
            setStatus(m_resumePhase);
            return;
        }
        if (m_phase == Phase::Searching || m_phase == Phase::Replaying ||
            m_phase == Phase::WaitingToStart || m_phase == Phase::WaitingAfterFailure) {
            m_resumePhase = m_phase;
            setStatus(Phase::Paused);
        }
    }

    void PathfinderController::stop(PlayLayer* playLayer) {
        auto shouldSaveFrontier = m_phase == Phase::Searching || m_phase == Phase::WaitingToStart ||
            m_phase == Phase::WaitingAfterFailure || (m_phase == Phase::Paused && m_resumePhase != Phase::Replaying);
        m_input.releaseAll(playLayer);
        m_gameState.clear();
        setStatus(Phase::Stopped);
        m_pendingReplay = false;
        if (shouldSaveFrontier && m_saveSearch && m_levelID != 0 && m_search.depth() > 0) {
            m_search.save(m_levelID);
        }
    }

    void PathfinderController::cancelSearch(PlayLayer* playLayer) {
        stop(playLayer);
    }

    void PathfinderController::reset(PlayLayer* playLayer) {
        if (!playLayer || !playLayer->m_level) {
            return;
        }
        m_input.releaseAll(playLayer);
        m_gameState.clear();
        m_levelID = currentLevelID(playLayer);
        m_search.reset(readSearchOptions());
        m_search.eraseSavedState(m_levelID);
        m_route.clear();
        setStatus(Phase::Stopped);
        start(playLayer);
    }

    bool PathfinderController::saveRouteAs(std::string name) {
        if (!m_route.hasRoute() || !m_routeHasLevel || m_routeLevelID != m_levelID) return false;
        if (!m_route.saveAs(m_levelID, std::move(name))) return false;
        return true;
    }

    bool PathfinderController::loadNamedRoute(std::filesystem::path const& path, PlayLayer* playLayer) {
        if (!playLayer || !playLayer->m_level) return false;
        auto activeLevelID = currentLevelID(playLayer);
        m_route.clear();
        if (!m_route.load(path)) return false;
        if (m_route.levelID() != activeLevelID) {
            geode::log::warn("Saved Pathfinder route '{}' belongs to level {}, but level {} is open", m_route.name(), m_route.levelID(), activeLevelID);
            m_route.clear();
            return false;
        }
        m_levelID = activeLevelID;
        m_routeLevelID = activeLevelID;
        m_routeHasLevel = true;
        m_input.releaseAll(playLayer);
        m_gameState.clear();
        beginRestart(playLayer, true);
        return true;
    }

    std::vector<SavedRouteInfo> PathfinderController::savedRoutes() const {
        return RouteLibrary::list();
    }

    void PathfinderController::onLayerExit(PlayLayer* playLayer) {
        auto shouldSave = m_phase == Phase::Searching || m_phase == Phase::WaitingToStart ||
            m_phase == Phase::WaitingAfterFailure ||
            (m_phase == Phase::Paused && m_resumePhase != Phase::Replaying);
        m_input.releaseAll(playLayer);
        if (shouldSave) {
            saveSearchIfEnabled();
        }
        m_gameState.clear();
        setStatus(Phase::Stopped);
        m_pendingReplay = false;
        m_route.clear();
        m_routeHasLevel = false;
    }

    ControllerSnapshot PathfinderController::snapshot() const {
        ControllerSnapshot result;
        switch (m_phase) {
            case Phase::Stopped: result.status = "Stopped"; break;
            case Phase::WaitingToStart: result.status = "Restarting level"; break;
            case Phase::Searching: result.status = "Searching"; break;
            case Phase::WaitingAfterFailure: result.status = "Branch failed; preparing next attempt"; break;
            case Phase::Replaying: result.status = "Replaying route"; break;
            case Phase::Paused: result.status = "Paused"; break;
            case Phase::Success: result.status = "Solution found"; break;
            case Phase::Replayed: result.status = "Replay completed"; break;
            case Phase::Exhausted: result.status = "Search space exhausted"; break;
            case Phase::AttemptLimit: result.status = "Attempt limit reached"; break;
            case Phase::ReplayFailed: result.status = "Replay failed"; break;
            case Phase::ResetFailed: result.status = "Level reset failed"; break;
            case Phase::RouteCaptureFailed: result.status = "Solution found, but its input path could not be saved"; break;
        }
        result.gameMode = m_currentMode;
        result.progress = m_currentProgress;
        result.bestProgress = m_search.bestProgress();
        result.attempts = m_search.attempts();
        result.maxAttempts = m_search.maxAttempts();
        result.frameResolution = m_search.frameResolution();
        result.currentDepth = static_cast<int>(m_cursorDepth);
        result.maxDepth = m_search.maxDepth();
        result.searchStates = m_search.searchStates();
        result.estimatedStates = m_search.estimatedStateCount();
        result.paused = m_phase == Phase::Paused;
        result.active = m_phase == Phase::Searching || m_phase == Phase::Replaying ||
            m_phase == Phase::WaitingToStart || m_phase == Phase::WaitingAfterFailure || m_phase == Phase::Paused;
        result.routeAvailable = m_route.hasRoute();
        return result;
    }

    bool PathfinderController::shouldFreeze(PlayLayer* playLayer) const {
        return playLayer && playLayer == PlayLayer::get() && m_phase == Phase::Paused;
    }

    void PathfinderController::setStatus(Phase phase) {
        m_phase = phase;
    }

    void PathfinderController::saveSearchIfEnabled() {
        if (m_saveSearch && m_levelID != 0) {
            m_search.save(m_levelID);
        }
    }
}
