#include "DeathCompletionDetector.hpp"

namespace upf {
    bool DeathCompletionDetector::playerDied(PlayLayer* playLayer) const {
        if (!playLayer || !playLayer->m_player1) {
            return false;
        }
        auto firstDead = playLayer->m_player1->m_isDead;
        auto secondDead = playLayer->m_player2 && playLayer->m_player2->isVisible() && playLayer->m_player2->m_isDead;
        return firstDead || secondDead;
    }

    bool DeathCompletionDetector::levelCompleted(PlayLayer* playLayer) const {
        return playLayer && playLayer->m_hasCompletedLevel;
    }

    float DeathCompletionDetector::progress(PlayLayer* playLayer) const {
        return playLayer ? playLayer->getCurrentPercent() : 0.f;
    }

    std::string DeathCompletionDetector::gameMode(PlayLayer* playLayer) const {
        if (!playLayer || !playLayer->m_player1) {
            return "Unknown";
        }

        auto player = playLayer->m_player1;
        std::string mode = "Cube";
        if (player->m_isShip) mode = "Ship";
        else if (player->m_isBall) mode = "Ball";
        else if (player->m_isBird) mode = "UFO";
        else if (player->m_isDart) mode = "Wave";
        else if (player->m_isRobot) mode = "Robot";
        else if (player->m_isSpider) mode = "Spider";
        else if (player->m_isSwing) mode = "Swing";

        if (player->m_vehicleSize > 0.f && player->m_vehicleSize < 0.75f) {
            mode += " / Mini";
        }
        if (playLayer->m_player2 && playLayer->m_player2->isVisible()) {
            mode += " / Dual";
        }
        return mode;
    }
}
