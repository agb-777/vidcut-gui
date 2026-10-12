cmake_minimum_required(VERSION 3.21)

# The application version lives HERE and nowhere else. It is passed to the code
# as VIDCUT_APP_VERSION (title bar, About dialog, macOS bundle, ...).
project(VidCut VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# core: pure C++17, no Qt (keyframe analysis, planner, ffmpeg command lines)
# ---------------------------------------------------------------------------
add_library(vidcut_core STATIC
    src/core/AppInfo.h
    src/core/MediaInfo.h
    src/core/NumParse.h          src/core/NumParse.cpp
    src/core/TimeUtils.h         src/core/TimeUtils.cpp
    src/core/EncodeSettings.h    src/core/EncodeSettings.cpp
    src/core/EncoderProfile.h    src/core/EncoderProfile.cpp
    src/core/KeyframeAnalyzer.h  src/core/KeyframeAnalyzer.cpp
    src/core/CutPlanner.h        src/core/CutPlanner.cpp
    src/core/PipelineBuilder.h   src/core/PipelineBuilder.cpp
)
target_include_directories(vidcut_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(vidcut_core PUBLIC
    VIDCUT_APP_NAME="${PROJECT_NAME}"
    VIDCUT_APP_VERSION="${PROJECT_VERSION}")

# ---------------------------------------------------------------------------
# GUI (Qt 6 Widgets)
# ---------------------------------------------------------------------------
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTOMOC ON)

qt_add_executable(VidCut WIN32 MACOSX_BUNDLE
    src/main.cpp

    src/engine/CutErrors.h
    src/engine/ProcessRunner.h    src/engine/ProcessRunner.cpp
    src/engine/FfmpegLocator.h    src/engine/FfmpegLocator.cpp
    src/engine/MediaProbe.h       src/engine/MediaProbe.cpp
    src/engine/DurationProbe.h    src/engine/DurationProbe.cpp
    src/engine/FileUtils.h        src/engine/FileUtils.cpp
    src/engine/SettingsStore.h    src/engine/SettingsStore.cpp
    src/engine/SmartCutter.h      src/engine/SmartCutter.cpp

    src/ui/MainWindow.h           src/ui/MainWindow.cpp
    src/ui/SettingsPanel.h        src/ui/SettingsPanel.cpp
    src/ui/AboutDialog.h          src/ui/AboutDialog.cpp
)
target_link_libraries(VidCut PRIVATE vidcut_core Qt6::Widgets)

set_target_properties(VidCut PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "VidCut"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.vidcut.app"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

include(GNUInstallDirs)
install(TARGETS VidCut
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
