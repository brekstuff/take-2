#pragma once

#include <Geode/binding/PlayerObject.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace upf {
    constexpr std::uint8_t kJumpMask = 1;
    constexpr std::uint8_t kLeftMask = 2;
    constexpr std::uint8_t kRightMask = 4;

    struct SearchOptions {
        int frameResolution = 4;
        int maxDepth = 12000;
        int maxAttempts = 10000;
        int aggressiveness = 5;
    };

    struct DecisionNode {
        std::array<std::uint8_t, 6> choices {};
        std::uint8_t choiceCount = 0;
        std::uint8_t selected = 0;
    };

    enum class BacktrackResult {
        NextBranch,
        RefinedResolution,
        Exhausted,
    };

    class SearchEngine {
    public:
        void reset(SearchOptions options);
        bool load(int levelID, SearchOptions options);
        bool save(int levelID) const;
        bool eraseSavedState(int levelID) const;

        bool ensureDecision(std::size_t depth, PlayerObject* player);
        [[nodiscard]] std::uint8_t actionAt(std::size_t depth) const;
        BacktrackResult advanceAfterFailure();
        void noteAttemptStarted();
        void updateProgress(float progress);

        [[nodiscard]] int frameResolution() const { return m_frameResolution; }
        [[nodiscard]] int maxDepth() const { return m_maxDepth; }
        [[nodiscard]] int maxAttempts() const { return m_maxAttempts; }
        [[nodiscard]] int attempts() const { return m_attempts; }
        [[nodiscard]] float bestProgress() const { return m_bestProgress; }
        [[nodiscard]] std::uint64_t searchStates() const { return m_searchStates; }
        [[nodiscard]] std::size_t depth() const { return m_path.size(); }
        [[nodiscard]] std::uint64_t estimatedStateCount() const;
        [[nodiscard]] const std::vector<DecisionNode>& path() const { return m_path; }
        [[nodiscard]] bool atAttemptLimit() const { return m_attempts >= m_maxAttempts; }

    private:
        static DecisionNode makeDecision(PlayerObject* player, int aggressiveness);
        std::vector<DecisionNode> m_path;
        int m_frameResolution = 4;
        int m_maxDepth = 12000;
        int m_maxAttempts = 10000;
        int m_aggressiveness = 5;
        int m_attempts = 0;
        float m_bestProgress = 0.f;
        std::uint64_t m_searchStates = 0;
    };
}
