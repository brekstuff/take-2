#include "SearchEngine.hpp"
#include "Storage.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <utility>

namespace upf {
    namespace {
        constexpr char kSearchMagic[] = "UPF_SEARCH";
        constexpr int kSearchVersion = 1;
        constexpr std::size_t kMaximumSavedDepth = 100000;
    }

    void SearchEngine::reset(SearchOptions options) {
        m_path.clear();
        m_frameResolution = std::clamp(options.frameResolution, 1, 24);
        m_maxDepth = std::clamp(options.maxDepth, 1, 100000);
        m_maxAttempts = std::clamp(options.maxAttempts, 1, 1000000);
        m_aggressiveness = std::clamp(options.aggressiveness, 1, 10);
        m_attempts = 0;
        m_bestProgress = 0.f;
        m_searchStates = 0;
    }

    bool SearchEngine::load(int levelID, SearchOptions options) {
        auto path = storage::searchPath(levelID);
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return false;
        }

        std::string magic;
        int version = 0;
        int savedLevelID = 0;
        int resolution = 0;
        int attempts = 0;
        float bestProgress = 0.f;
        std::uint64_t states = 0;
        std::uint64_t depth = 0;
        if (!(file >> magic >> version) || magic != kSearchMagic || version != kSearchVersion ||
            !(file >> savedLevelID >> resolution >> attempts >> bestProgress >> states >> depth) ||
            savedLevelID != levelID || resolution < 1 || resolution > 24 || attempts < 0 ||
            !std::isfinite(bestProgress) || bestProgress < 0.f || bestProgress > 100.f || depth > kMaximumSavedDepth ||
            depth > static_cast<std::uint64_t>(std::clamp(options.maxDepth, 1, 100000))) {
            geode::log::warn("Rejected invalid Pathfinder search state: {}", path.string());
            return false;
        }

        std::vector<DecisionNode> loaded;
        loaded.reserve(static_cast<std::size_t>(depth));
        for (std::uint64_t i = 0; i < depth; ++i) {
            unsigned int count = 0;
            unsigned int selected = 0;
            if (!(file >> count >> selected) || count == 0 || count > 6 || selected >= count) {
                geode::log::warn("Rejected corrupted Pathfinder search state: {}", path.string());
                return false;
            }

            DecisionNode node;
            node.choiceCount = static_cast<std::uint8_t>(count);
            node.selected = static_cast<std::uint8_t>(selected);
            for (unsigned int j = 0; j < count; ++j) {
                unsigned int choice = 0;
                if (!(file >> choice) || choice > 7) {
                    geode::log::warn("Rejected corrupted Pathfinder search state: {}", path.string());
                    return false;
                }
                node.choices[j] = static_cast<std::uint8_t>(choice);
            }
            loaded.push_back(node);
        }

        file >> std::ws;
        if (!file.eof()) {
            geode::log::warn("Rejected trailing data in Pathfinder search state: {}", path.string());
            return false;
        }

        m_path = std::move(loaded);
        m_frameResolution = resolution;
        m_maxDepth = std::clamp(options.maxDepth, 1, 100000);
        m_maxAttempts = std::clamp(options.maxAttempts, 1, 1000000);
        m_aggressiveness = std::clamp(options.aggressiveness, 1, 10);
        m_attempts = attempts;
        m_bestProgress = bestProgress;
        m_searchStates = states;
        geode::log::info("Resuming Pathfinder frontier for level {} at depth {}", levelID, m_path.size());
        return true;
    }

    bool SearchEngine::save(int levelID) const {
        auto path = storage::searchPath(levelID);
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            geode::log::error("Could not open Pathfinder search state for writing: {}", path.string());
            return false;
        }

        file << kSearchMagic << ' ' << kSearchVersion << '\n';
        file << levelID << ' ' << m_frameResolution << ' ' << m_attempts << ' '
             << std::setprecision(7) << m_bestProgress << ' ' << m_searchStates << ' ' << m_path.size() << '\n';
        for (auto const& node : m_path) {
            file << static_cast<unsigned int>(node.choiceCount) << ' '
                 << static_cast<unsigned int>(node.selected);
            for (std::uint8_t i = 0; i < node.choiceCount; ++i) {
                file << ' ' << static_cast<unsigned int>(node.choices[i]);
            }
            file << '\n';
        }
        file.flush();
        if (!file) {
            geode::log::error("Failed while writing Pathfinder search state: {}", path.string());
            return false;
        }
        return true;
    }

    bool SearchEngine::eraseSavedState(int levelID) const {
        std::error_code error;
        auto removed = std::filesystem::remove(storage::searchPath(levelID), error);
        if (error) {
            geode::log::warn("Could not remove saved Pathfinder search state: {}", error.message());
            return false;
        }
        return removed;
    }

    bool SearchEngine::ensureDecision(std::size_t depth, PlayerObject* player) {
        if (depth < m_path.size()) {
            return true;
        }
        if (depth != m_path.size() || depth >= static_cast<std::size_t>(m_maxDepth) || !player) {
            return false;
        }

        // A node is one input mask held for frameResolution game updates. Repeated nodes
        // express press/hold/release/wait timings; the game itself resolves all physics.
        auto node = makeDecision(player, m_aggressiveness);
        m_path.push_back(node);
        ++m_searchStates;
        return true;
    }

    std::uint8_t SearchEngine::actionAt(std::size_t depth) const {
        if (depth >= m_path.size()) {
            return 0;
        }
        auto const& node = m_path[depth];
        return node.choices[node.selected];
    }

    BacktrackResult SearchEngine::advanceAfterFailure() {
        // Depth-first traversal encodes exhausted choices in each node's selected index.
        // That compact frontier is also what save/load persists between runs.
        for (std::size_t remaining = m_path.size(); remaining > 0; --remaining) {
            auto index = remaining - 1;
            m_path.resize(index + 1);
            auto& node = m_path[index];
            if (static_cast<std::size_t>(node.selected) + 1 < node.choiceCount) {
                ++node.selected;
                ++m_searchStates;
                return BacktrackResult::NextBranch;
            }
        }

        m_path.clear();
        if (m_frameResolution > 1) {
            m_frameResolution = std::max(1, (m_frameResolution + 1) / 2);
            ++m_searchStates;
            geode::log::info("Refining Pathfinder input resolution to {} update(s)", m_frameResolution);
            return BacktrackResult::RefinedResolution;
        }
        return BacktrackResult::Exhausted;
    }

    void SearchEngine::noteAttemptStarted() {
        ++m_attempts;
    }

    void SearchEngine::updateProgress(float progress) {
        if (std::isfinite(progress)) {
            m_bestProgress = std::max(m_bestProgress, std::clamp(progress, 0.f, 100.f));
        }
    }

    std::uint64_t SearchEngine::estimatedStateCount() const {
        long double product = 1.0L;
        long double total = 1.0L;
        for (auto const& node : m_path) {
            product *= std::max<unsigned int>(2, node.choiceCount);
            total += product;
            if (total >= static_cast<long double>(std::numeric_limits<std::uint64_t>::max())) {
                return std::numeric_limits<std::uint64_t>::max();
            }
        }
        return static_cast<std::uint64_t>(total);
    }

    DecisionNode SearchEngine::makeDecision(PlayerObject* player, int aggressiveness) {
        DecisionNode node;
        if (player->m_isPlatformer) {
            node.choiceCount = 6;
            if (aggressiveness >= 6) {
                node.choices = { static_cast<std::uint8_t>(kRightMask | kJumpMask), kRightMask,
                    kJumpMask, 0, static_cast<std::uint8_t>(kLeftMask | kJumpMask), kLeftMask };
            }
            else {
                node.choices = { kRightMask, static_cast<std::uint8_t>(kRightMask | kJumpMask),
                    0, kJumpMask, kLeftMask, static_cast<std::uint8_t>(kLeftMask | kJumpMask) };
            }
        }
        else {
            node.choiceCount = 2;
            bool preferHold = player->m_isShip || player->m_isBird || player->m_isDart || player->m_isSwing;
            if (aggressiveness >= 6) {
                preferHold = !preferHold;
            }
            node.choices[0] = preferHold ? kJumpMask : 0;
            node.choices[1] = preferHold ? 0 : kJumpMask;
        }
        return node;
    }

}
