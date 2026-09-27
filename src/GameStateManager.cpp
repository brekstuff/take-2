#include "GameStateManager.hpp"

namespace upf {
    bool GameStateManager::restartFromBeginning(PlayLayer* playLayer) {
        if (!playLayer) {
            return false;
        }
        m_waiting = true;
        m_waitTicks = 0;
        // GD's public bindings do not expose a complete, restorable snapshot for triggers and moving objects.
        // Reset through the game and replay the retained input prefix to recreate the branch state.
        playLayer->resetLevel();
        return true;
    }

    FreshStartResult GameStateManager::waitForFreshStart(PlayLayer* playLayer, bool playerDead) {
        if (!m_waiting) {
            return FreshStartResult::Ready;
        }
        if (!playLayer) {
            return FreshStartResult::Failed;
        }

        ++m_waitTicks;
        if (playerDead) {
            if (m_waitTicks >= 180) {
                m_waiting = false;
                m_waitTicks = 0;
                geode::log::error("Pathfinder could not revive the player after resetting the level");
                return FreshStartResult::Failed;
            }
            return FreshStartResult::Waiting;
        }
        if (m_waitTicks >= 1 && playLayer->m_attemptTime <= 0.08) {
            m_waiting = false;
            m_waitTicks = 0;
            return FreshStartResult::Ready;
        }
        if (m_waitTicks >= 180) {
            geode::log::warn("Pathfinder reset did not report a fresh attempt time; continuing from the current live state");
            m_waiting = false;
            m_waitTicks = 0;
            return FreshStartResult::Ready;
        }
        return FreshStartResult::Waiting;
    }

    void GameStateManager::clear() {
        m_waiting = false;
        m_waitTicks = 0;
    }
}
