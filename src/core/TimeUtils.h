#pragma once
#include <cstdint>
#include <string>

namespace vidcut {

// seconds -> "HH:MM:SS.mmm"
std::string formatTime(double seconds);

// Fixed-point number, always with '.' as decimal separator (locale independent).
std::string formatFixed(double value, int decimals);

// ffmpeg time argument, e.g. "12.345000".
std::string formatSeconds(double seconds);

// Human readable size: "512 B", "1.5 MB", ...
std::string formatSize(std::uint64_t bytes);

}  // namespace vidcut
