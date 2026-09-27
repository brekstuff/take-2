cmake_minimum_required(VERSION 3.29)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(UniversalPathfinder VERSION 1.0.0 LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK is not set. Install the Geode SDK and set GEODE_SDK to its directory.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})

# Keep the requested package name stable even though Geode names the package
# from the mod ID. This target is built after the regular Geode package step.
add_custom_target(UniversalPathfinder_DIST
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/universal.pathfinder.geode"
        "${CMAKE_CURRENT_BINARY_DIR}/UniversalPathfinder.geode"
    DEPENDS ${PROJECT_NAME}_PACKAGE
    COMMENT "Copying the package to UniversalPathfinder.geode"
)
