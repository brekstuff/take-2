# Universal Pathfinder

Universal Pathfinder is a Windows Geode mod targeting Geometry Dash 2.2081. During a search it tests candidate input sequences by stepping the real game. A candidate consists of timed button states; after failure, it resets the level and replays the retained prefix before trying the next branch.

## In-game controls

- Press **Left Alt** in a level to open the menu.
- **Pathfind** opens a black progress screen. It shows the current level percent and has a **Cancel** button. When a complete route is found, the mod asks for a name and saves it.
- **Load** opens a scrollable list of saved routes. Selecting a route starts its replay when the matching level is open.

The Left Alt binding can be changed in Geode's keybind settings. Search limits and timing resolution remain available in the mod settings.

## Saved routes

Named route files are stored in:

```text
<Geode save directory>\universal-pathfinder\solutions\
```

Each file includes its route name, level ID, run length, and input changes. Existing unnamed route files from the earlier project format remain readable and appear as `Level <id>` in the list. Route data is validated before playback.

## Search limits

The search uses Geometry Dash's own 2.2081 update and input routines, so the route trial follows the live game's physics and 2.2 triggers. The current search is still heuristic: it only tests a small set of jump/left/right input masks at the configured timing resolution, with one game simulation running at a time. Dual players share those controls. It does not guarantee a solution for arbitrary levels, and playback can diverge on nondeterministic content.

This project does not yet run an external high-speed simulator. The Pathfinder reference repository contains a separate `gd-sim` implementation, but its README says full physics support only goes through 1.7, with partial support through 1.9, and says the simulator is not licensed for redistribution. It therefore cannot be bundled as a faithful 2.2 engine. A new simulator with 2.2 trigger and object behavior would need to be implemented and validated independently before it could safely replace live-game trials.

## Build

Requirements:

- Geometry Dash 2.2081 and Geode SDK v5.10.1.
- CMake 3.29+, Git, and a Geode-supported C++23 Windows toolchain.
- `GEODE_SDK` set to the SDK directory.

Build with:

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DGEODE_DONT_INSTALL_MODS=ON
cmake --build build --config RelWithDebInfo --target UniversalPathfinder_DIST
```

The requested package name is `build/UniversalPathfinder.geode`. The GitHub Actions workflow also publishes that file as a downloadable artifact.

## Install

Install `UniversalPathfinder.geode` using the Geode CLI or place it in:

```text
<Geometry Dash installation>\geode\mods
```

Restart Geometry Dash, open a level, and press Left Alt. The source package and installed mod are separate; rebuilding this source does not update an already installed Pathfinder mod.
