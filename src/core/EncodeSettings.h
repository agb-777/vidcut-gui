#pragma once
#include <string>
#include <vector>

namespace vidcut {

// User-configurable encoding options (nothing is hardcoded in the encoder code).
struct EncodeSettings {
    std::string preset = "ultrafast";
    int crf = 20;
    std::string audioBitrate = "192k";
};

constexpr int kCrfMin = 0;
constexpr int kCrfMax = 51;

EncodeSettings defaultSettings();
const std::vector<std::string>& presetList();
const std::vector<std::string>& audioBitrateChoices();

// Returns an empty string when valid, otherwise a human readable error message.
std::string validateSettings(const EncodeSettings& s);

}  // namespace vidcut
