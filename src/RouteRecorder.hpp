#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace upf {
    struct RouteEvent {
        std::uint64_t frame = 0;
        std::uint8_t mask = 0;
    };

    struct SavedRouteInfo {
        std::string name;
        int levelID = 0;
        std::uint64_t totalFrames = 0;
        std::filesystem::path file;
    };

    class RouteLibrary {
    public:
        static std::vector<SavedRouteInfo> list();
        static std::filesystem::path createFilePath(std::string const& name, int levelID);
    };

    class RouteRecorder {
    public:
        void beginRecording();
        void recordInput(std::uint64_t frame, std::uint8_t mask);
        bool finishRecording(std::uint64_t totalFrames);
        bool saveAs(int levelID, std::string name);
        bool load(std::filesystem::path const& path);
        void beginReplay();
        void clear();

        [[nodiscard]] const std::vector<RouteEvent>& events() const { return m_events; }
        [[nodiscard]] std::uint64_t totalFrames() const { return m_totalFrames; }
        [[nodiscard]] std::size_t replayCursor() const { return m_replayCursor; }
        void setReplayCursor(std::size_t cursor) { m_replayCursor = cursor; }
        [[nodiscard]] bool hasRoute() const { return m_hasRoute; }
        [[nodiscard]] int levelID() const { return m_levelID; }
        [[nodiscard]] std::string const& name() const { return m_name; }

    private:
        bool write(std::filesystem::path const& path);

        std::vector<RouteEvent> m_events;
        std::string m_name;
        int m_levelID = 0;
        std::uint64_t m_totalFrames = 0;
        std::size_t m_replayCursor = 0;
        std::uint8_t m_lastMask = 0;
        bool m_recording = false;
        bool m_hasRoute = false;
        bool m_overflowed = false;
    };
}
