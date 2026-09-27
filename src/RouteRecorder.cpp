#include "RouteRecorder.hpp"
#include "Storage.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace upf {
    namespace {
        constexpr std::uint64_t kMaximumRouteFrames = 50'000'000;
        constexpr std::size_t kMaximumRouteEvents = 5'000'000;
        constexpr char kRouteMagic[] = "UPF_ROUTE";
        constexpr int kRouteVersion = 2;

        std::string cleanName(std::string name) {
            auto first = name.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};
            auto last = name.find_last_not_of(" \t\r\n");
            name = name.substr(first, last - first + 1);
            if (name.size() > 64) name.resize(64);
            return name;
        }

        std::string makeSlug(std::string const& name) {
            std::string slug;
            bool lastWasDash = false;
            for (unsigned char ch : name) {
                if (std::isalnum(ch)) {
                    slug.push_back(static_cast<char>(std::tolower(ch)));
                    lastWasDash = false;
                }
                else if (!slug.empty() && !lastWasDash) {
                    slug.push_back('-');
                    lastWasDash = true;
                }
                if (slug.size() >= 36) break;
            }
            while (!slug.empty() && slug.back() == '-') slug.pop_back();
            return slug.empty() ? "solution" : slug;
        }

        bool readHeader(std::ifstream& file, SavedRouteInfo& info, int version) {
            std::uint64_t eventCount = 0;
            if (version == 1) {
                if (!(file >> info.levelID >> info.totalFrames >> eventCount)) return false;
                info.name = "Level " + std::to_string(info.levelID);
            }
            else if (version == kRouteVersion) {
                if (!(file >> std::quoted(info.name) >> info.levelID >> info.totalFrames >> eventCount)) return false;
                info.name = cleanName(std::move(info.name));
            }
            else {
                return false;
            }

            return !info.name.empty() && info.levelID >= 0 &&
                info.totalFrames <= kMaximumRouteFrames && eventCount <= kMaximumRouteEvents;
        }
    }

    std::vector<SavedRouteInfo> RouteLibrary::list() {
        std::vector<SavedRouteInfo> routes;
        auto directory = storage::solutionsDirectory();
        std::error_code error;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            if (!it->is_regular_file(error) || it->path().extension() != ".upfroute") continue;
            std::ifstream file(it->path(), std::ios::binary);
            std::string magic;
            int version = 0;
            SavedRouteInfo info;
            if (!file || !(file >> magic >> version) || magic != kRouteMagic || !readHeader(file, info, version)) {
                geode::log::warn("Skipping an invalid saved Pathfinder route: {}", it->path().string());
                continue;
            }
            info.file = it->path();
            routes.push_back(std::move(info));
        }
        if (error) {
            geode::log::warn("Could not list Pathfinder routes: {}", error.message());
        }
        std::sort(routes.begin(), routes.end(), [](auto const& left, auto const& right) {
            if (left.name != right.name) return left.name < right.name;
            return left.levelID < right.levelID;
        });
        return routes;
    }

    std::filesystem::path RouteLibrary::createFilePath(std::string const& name, int levelID) {
        auto directory = storage::solutionsDirectory();
        auto stem = makeSlug(name) + "-level-" + std::to_string(levelID);
        for (std::uint64_t suffix = 1; suffix < std::numeric_limits<std::uint64_t>::max(); ++suffix) {
            auto candidate = directory / (stem + "-" + std::to_string(suffix) + ".upfroute");
            std::error_code error;
            if (!std::filesystem::exists(candidate, error) && !error) return candidate;
        }
        return directory / (stem + "-fallback.upfroute");
    }

    void RouteRecorder::beginRecording() {
        m_events.clear();
        m_name.clear();
        m_levelID = 0;
        m_totalFrames = 0;
        m_replayCursor = 0;
        m_lastMask = 0;
        m_recording = true;
        m_hasRoute = false;
        m_overflowed = false;
    }

    void RouteRecorder::recordInput(std::uint64_t frame, std::uint8_t mask) {
        if (!m_recording || mask > 7 || mask == m_lastMask) {
            return;
        }

        if (m_events.size() >= kMaximumRouteEvents) {
            geode::log::warn("Pathfinder route reached its event limit; further input changes are not recorded");
            m_recording = false;
            m_overflowed = true;
            return;
        }

        m_events.push_back(RouteEvent { frame, mask });
        m_lastMask = mask;
    }

    bool RouteRecorder::finishRecording(std::uint64_t totalFrames) {
        m_recording = false;
        m_totalFrames = totalFrames;
        m_hasRoute = !m_overflowed && totalFrames <= kMaximumRouteFrames;
        if (!m_hasRoute) {
            geode::log::error("Pathfinder found a route but could not keep a complete input sequence");
        }
        return m_hasRoute;
    }

    bool RouteRecorder::saveAs(int levelID, std::string name) {
        if (!m_hasRoute) return false;
        name = cleanName(std::move(name));
        if (name.empty() || levelID < 0) return false;
        auto path = RouteLibrary::createFilePath(name, levelID);
        m_name = std::move(name);
        m_levelID = levelID;
        return write(path);
    }

    bool RouteRecorder::write(std::filesystem::path const& path) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) {
            geode::log::error("Could not open route file for writing: {}", path.string());
            return false;
        }

        file << kRouteMagic << ' ' << kRouteVersion << '\n';
        file << std::quoted(m_name) << ' ' << m_levelID << ' ' << m_totalFrames << ' ' << m_events.size() << '\n';
        for (auto const& event : m_events) {
            file << event.frame << ' ' << static_cast<unsigned int>(event.mask) << '\n';
        }
        file.flush();
        if (!file) {
            geode::log::error("Failed while writing route file: {}", path.string());
            return false;
        }
        geode::log::info("Saved Pathfinder route '{}' for level {} ({} input changes): {}", m_name, m_levelID, m_events.size(), path.string());
        return true;
    }

    bool RouteRecorder::load(std::filesystem::path const& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        std::string magic;
        int version = 0;
        SavedRouteInfo info;
        if (!(file >> magic >> version) || magic != kRouteMagic || !readHeader(file, info, version)) {
            geode::log::warn("Rejected invalid or incompatible Pathfinder route: {}", path.string());
            return false;
        }

        std::uint64_t eventCount = 0;
        if (version == 1) {
            file.clear();
            file.seekg(0);
            file >> magic >> version >> info.levelID >> info.totalFrames >> eventCount;
        }
        else {
            std::string ignoredName;
            file.clear();
            file.seekg(0);
            file >> magic >> version >> std::quoted(ignoredName) >> info.levelID >> info.totalFrames >> eventCount;
        }
        if (!file || eventCount > kMaximumRouteEvents) return false;

        std::vector<RouteEvent> parsed;
        parsed.reserve(static_cast<std::size_t>(eventCount));
        std::uint64_t previousFrame = 0;
        for (std::uint64_t i = 0; i < eventCount; ++i) {
            std::uint64_t frame = 0;
            unsigned int mask = 0;
            if (!(file >> frame >> mask) || frame > info.totalFrames || mask > 7 ||
                (i > 0 && frame <= previousFrame)) {
                geode::log::warn("Rejected corrupted Pathfinder route: {}", path.string());
                return false;
            }
            parsed.push_back(RouteEvent { frame, static_cast<std::uint8_t>(mask) });
            previousFrame = frame;
        }

        file >> std::ws;
        if (!file.eof()) {
            geode::log::warn("Rejected trailing data in Pathfinder route: {}", path.string());
            return false;
        }

        m_events = std::move(parsed);
        m_name = cleanName(std::move(info.name));
        m_levelID = info.levelID;
        m_totalFrames = info.totalFrames;
        m_replayCursor = 0;
        m_lastMask = m_events.empty() ? 0 : m_events.back().mask;
        m_recording = false;
        m_hasRoute = true;
        m_overflowed = false;
        geode::log::info("Loaded Pathfinder route '{}' for level {} ({} input changes)", m_name, m_levelID, m_events.size());
        return true;
    }

    void RouteRecorder::beginReplay() {
        m_replayCursor = 0;
    }

    void RouteRecorder::clear() {
        m_events.clear();
        m_name.clear();
        m_levelID = 0;
        m_totalFrames = 0;
        m_replayCursor = 0;
        m_lastMask = 0;
        m_recording = false;
        m_hasRoute = false;
        m_overflowed = false;
    }
}
