#pragma once

// Application name and version. The macros are injected by CMake from
// project(VidCut VERSION x.y.z), so the version is defined in ONE place only.
#ifndef VIDCUT_APP_NAME
#define VIDCUT_APP_NAME "VidCut"
#endif
#ifndef VIDCUT_APP_VERSION
#define VIDCUT_APP_VERSION "0.0.0"
#endif

namespace vidcut::appinfo {
constexpr const char* name = VIDCUT_APP_NAME;
constexpr const char* version = VIDCUT_APP_VERSION;
}  // namespace vidcut::appinfo
