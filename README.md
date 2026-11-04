# TempVisualizer

A high-performance, standalone Windows application coded in C++ that visualizes real-time temperature data from structural log streams on a hardware-accelerated interactive graph.

Built using **Dear ImGui**, **ImPlot**, **GLFW**, and **OpenGL3**, the application allows users to load text logs via a native Windows file selection dialogue, parsing custom log payloads natively to display temperature data across absolute historical timelines (24-hour formatting).

---

## Key Features

*   **Interactive Charting:** Dynamic, hardware-accelerated timeline graph supporting native **Left-Click + Drag to Pan** and **Right-Click / Mouse-Wheel to Zoom**.
*   **Native File Browser:** Built-in Windows file selector dialogue (`GetOpenFileNameA`) to dynamically shift target telemetry files seamlessly on the fly.
*   **Live Stream Engine:** Non-blocking file-reading pipeline that processes appended logs dynamically without creating exclusive operating system locks.
*   **Custom Data Extraction:** Custom, optimized substring string-parser targeting the exact JSON-like machine telemetry payload (extracting timestamp signatures and `TempR2` room metrics, with logged temperature values converted from tenths of a degree).
*   **Chronological Tracking:** Absolute Unix Epoch baseline conversion providing proper timeline grouping and structural standard 24-hour axis label scales (`%m/%d %H:%M`).

---

## Development Toolchain Setup

Setting up a working MinGW C++ graphical compiler environment from scratch under Windows requires specific package linkages. Below is the precise pathing blueprint executed to configure this environment:

### 1. The Compiler Engine (MinGW-w64 via MSYS2)
1. Downloaded and deployed the environment wrapper framework from [msys2.org](https://msys2.org).
2. Ran the compilation toolchain synchronization routine:
   ```bash
   pacman -S --needed base-devel mingw-w64-ucrt-x86_64-toolchain
   ```
3. Added the explicit binary variable location to the **very top** of the Windows Environment System `Path` variable directory stack to prevent dependency pathing conflicts:
   ```text
   C:\msys64\ucrt64\bin
   ```

### 2. Dependency Allocation (vcpkg)
Managed via Microsoft's official C++ package manager to isolate library builds under MinGW targets rather than default MSVC Visual Studio engines:
```powershell
cd C:\
git clone https://github.com
cd vcpkg
.\bootstrap-vcpkg.bat

# Enforce explicit MinGW compilation triplets for both target and host configurations
.\vcpkg install glfw3 opengl imgui[glfw-binding,opengl3-binding] implot --triplet=x64-mingw-dynamic --host-triplet=x64-mingw-dynamic
```

### 3. Integrated Development Environment (VS Code)
1. Deployed Microsoft **C/C++** and **CMake Tools** extensions.
2. Mapped the explicit execution path directly to the local MSYS2 internal wrapper inside global settings (`cmake.cmakePath`):
   ```text
   C:\msys64\ucrt64\bin\cmake.exe
   ```

---

## Project Structure & Build Configuration

The configuration maps dependencies through local vcpkg folders using static build anchors:

### `CMakeLists.txt`
```cmake
cmake_minimum_required(VERSION 3.10)
project(TemperatureVisualizer)

# Force the toolchain paths directly to your localized vcpkg path
set(CMAKE_TOOLCHAIN_FILE "C:/Users/House Wingrove/Downloads/vcpkg-2026.10.16/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "" FORCE)
set(CMAKE_PREFIX_PATH "C:/Users/House Wingrove/Downloads/vcpkg-2026.10.16/installed/x64-mingw-dynamic" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)

add_executable(TemperatureVisualizer main.cpp)

# Link the graphical engines and native Windows system dialog shells
target_link_libraries(TemperatureVisualizer PRIVATE 
    glfw
    imgui::imgui
    implot::implot
    opengl32
    comdlg32
)

# Automated script execution routine moving required DLLs directly into the build execution target folder
add_custom_command(TARGET TemperatureVisualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Users/House Wingrove/Downloads/vcpkg-2026.10.16/installed/x64-mingw-dynamic/bin/glfw3.dll"
    $<TARGET_FILE_DIR:TemperatureVisualizer>
)
```

---

## Compilation and Execution

1. Press `Ctrl + Shift + P` inside VS Code and execute: **`CMake: Delete Cache and Reconfigure`**.
2. Select the `GCC for x86_64-w64-mingw32` active kit framework profile.
3. Click the **`[Launch]`** status action play triangle button located along the bottom toolbar.

### Data Format Expectations
The application targets and continuously reads log structures matching this telemetry schema format natively. Temperature readings plotted from `TempR2` are in tenths of a degree and are divided by 10 for display (for example, `-6` is displayed as `-0.6°C`):
```text
2026-02-04 14:46:20 | Data | {"TempR2":63,"TempR3":1,"TempR4":-16}
2026-02-04 14:46:30 | Data | {"TempR2":62,"TempR3":0,"TempR4":-16}
```
