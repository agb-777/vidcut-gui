#include "core/EncodeSettings.h"

#include <algorithm>
#include <regex>

#include "core/NumParse.h"

namespace vidcut {

EncodeSettings defaultSettings() { return EncodeSettings{}; }

const std::vector<std::string>& presetList() {
    static const std::vector<std::string> v = {
        "ultrafast", "superfast", "veryfast", "faster", "fast",
        "medium",    "slow",      "slower",   "veryslow"};
    return v;
}

const std::vector<std::string>& audioBitrateChoices() {
    static const std::vector<std::string> v = {"96k", "128k", "160k",
                                               "192k", "256k", "320k"};
    return v;
}

std::string validateSettings(const EncodeSettings& s) {
    const auto& presets = presetList();
    if (std::find(presets.begin(), presets.end(), s.preset) == presets.end())
        return "Invalid preset '" + s.preset + "'.";

    if (s.crf < kCrfMin || s.crf > kCrfMax)
        return "CRF must be between " + std::to_string(kCrfMin) + " and " +
               std::to_string(kCrfMax) + ".";

    static const std::regex re(R"((\d+(?:\.\d+)?)([kKmM]?))");
    std::smatch m;
    if (!std::regex_match(s.audioBitrate, m, re))
        return "Invalid audio bitrate '" + s.audioBitrate +
               "'. Use a value like 192k, 320k or 1M.";
    double value = 0.0;
    parseDouble(m[1].str(), value);
    const std::string suffix = m[2].str();
    if (suffix == "k" || suffix == "K") value *= 1000.0;
    else if (suffix == "m" || suffix == "M") value *= 1000000.0;
    if (value < 8000.0 || value > 4000000.0)
        return "Audio bitrate must be between 8k and 4M.";
    return {};
}

}  // namespace vidcut
