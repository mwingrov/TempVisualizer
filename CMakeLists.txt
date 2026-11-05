cmake_minimum_required(VERSION 3.10)
project(TemperatureVisualizer)

# 1. Force the toolchain paths directly to your Downloads vcpkg directory
set(CMAKE_TOOLCHAIN_FILE "C:/Users/House Wingrove/Downloads/vcpkg-2026.10.16/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "" FORCE)

# 2. Tell CMake exactly where the compiled package binaries live
set(CMAKE_PREFIX_PATH "C:/Users/House Wingrove/Downloads/vcpkg-2026.10.16/installed/x64-mingw-dynamic" CACHE STRING "" FORCE)

# Tell CMake to use your C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate our installed libraries via vcpkg
find_package(glfw3 REQUIRED)
find_package(imgui REQUIRED)
find_package(implot REQUIRED)

# Define our standalone executable target binary
add_executable(TemperatureVisualizer main.cpp)

# Link the graphical dependencies straight into our program execution footprint
target_link_libraries(TemperatureVisualizer PRIVATE 
    glfw
    imgui::imgui
    implot::implot
    opengl32
    comdlg32
)

add_custom_command(TARGET TemperatureVisualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:glfw>"
    "$<TARGET_FILE_DIR:TemperatureVisualizer>"
)
