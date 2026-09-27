#pragma once

#include <Geode/Geode.hpp>
#include <filesystem>
#include <string>
#include <system_error>

namespace upf::storage {
    inline std::filesystem::path directory() {
        auto path = geode::Mod::get()->getSaveDir() / "universal-pathfinder";
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            geode::log::warn("Could not create Pathfinder save directory: {}", error.message());
        }
        return path;
    }

    inline std::filesystem::path solutionsDirectory() {
        auto path = directory() / "solutions";
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error) {
            geode::log::warn("Could not create Pathfinder solutions folder: {}", error.message());
        }
        return path;
    }

    inline std::filesystem::path searchPath(int levelID) {
        return directory() / ("search-" + std::to_string(levelID) + ".upfstate");
    }
}
