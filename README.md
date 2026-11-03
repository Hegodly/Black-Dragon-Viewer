# KAMI Beyond Reality — Original Core

KAMI is a new, independently authored Windows application. This project does not import Black Dragon, Firestorm or official Second Life viewer source, assets, skins, shaders, or build scripts.

## Current status
Original native Windows shell, Direct2D/DirectWrite interface, user-selectable light/dark appearances, keyboard navigation, and separate protocol boundary. This is NOT YET a working Second Life viewer: authentication, world rendering, inventory, avatar and messaging are not implemented.

## Build
Requires Windows, Visual Studio 2022 Desktop development with C++, Windows SDK, and CMake.
Run these commands in the project root:
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
Then start build/Release/KAMIViewer.exe.

F1-F6 changes screens; T toggles light/dark and saves the preference in the Windows user registry.

This clean source tree lives on a separate branch in the owner's existing GitHub repository. The old repository name and earlier branches remain until a new repository is created or the existing one renamed. No inherited viewer files are included in this branch's source tree. The user has not yet selected a license for their newly authored code.
