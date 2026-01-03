cmake_minimum_required(VERSION 3.20)

# Fix for dependencies (like FreeType) using older CMake policies
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW) # Start allowing option() to override cache variables
set(CMAKE_POLICY_VERSION_MINIMUM "3.5")

project(FntGenerator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Dependencies ---

# 1. GLFW (Windowing)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# 2. FreeType (Font Rendering)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://gitlab.freedesktop.org/freetype/freetype.git
    GIT_TAG        VER-2-13-2
)
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(freetype)

# 3. nlohmann_json (JSON)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(json)

# 4. Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90
)
FetchContent_MakeAvailable(imgui)

# 5. stb (Image Write)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# --- Source Files ---

# Create source structure if not exists, but CMake usually expects files to exist.
# We will define the target first and add sources.

# ImGui Sources (need to be compiled directly into the app)
set(IMGUI_DIR ${imgui_SOURCE_DIR})
list(APPEND IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# PlatformUtils
if(WIN32)
    list(APPEND PLATFORM_SOURCES src/Utils/PlatformUtils_Win32.cpp)
elseif(APPLE)
    list(APPEND PLATFORM_SOURCES src/Utils/PlatformUtils_macOS.mm)
else()
    list(APPEND PLATFORM_SOURCES src/Utils/PlatformUtils_Linux.cpp)
endif()

add_executable(FntGenerator WIN32
    src/main.cpp
    src/Font/FontManager.cpp
    src/Atlas/TextureGenerator.cpp
    src/Utils/Exporter.cpp
    src/Utils/BitmapUtils.cpp
    ${IMGUI_SOURCES}
    ${PLATFORM_SOURCES}
)

# --- Includes & Linking ---

target_include_directories(FntGenerator PRIVATE
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${stb_SOURCE_DIR}
)

target_link_libraries(FntGenerator PRIVATE
    glfw
    freetype
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(FntGenerator PRIVATE opengl32)
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(COREVIDEO_LIBRARY CoreVideo)
    find_library(QUARTZ_LIBRARY QuartzCore)
    target_link_libraries(FntGenerator PRIVATE 
        ${COCOA_LIBRARY} 
        ${IOKIT_LIBRARY} 
        ${COREVIDEO_LIBRARY} 
        ${QUARTZ_LIBRARY}
        "-framework OpenGL"
    )
else()
    target_link_libraries(FntGenerator PRIVATE GL dl pthread)
endif()

# Handle macOS Bundle and Metadata
if(APPLE)
    set_target_properties(FntGenerator PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Fnt Generator"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.renanjeron.fntgenerator"
        MACOSX_BUNDLE_BUNDLE_VERSION "1.0.0"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0.0"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026"
    )
endif()


# Copy assets if needed (placeholder)
# file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})
