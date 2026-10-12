#include "core/EncoderProfile.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace vidcut {

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static const std::map<std::string, std::string>& h264Profiles() {
    static const std::map<std::string, std::string> m = {
        {"baseline", "baseline"},
        {"constrained baseline", "baseline"},
        {"main", "main"},
        {"high", "high"},
        {"high 10", "high10"},
        {"high 4:2:2", "high422"},
        {"high 4:4:4 predictive", "high444"},
    };
    return m;
}

static const std::map<std::string, std::string>& h265Profiles() {
    static const std::map<std::string, std::string> m = {
        {"main", "main"},
        {"main 10", "main10"},
        {"main still picture", "main"},
    };
    return m;
}

std::optional<std::vector<std::string>> buildEncoderArgs(const VideoInfo& v,
                                                         const EncodeSettings& s) {
    const std::string profile = lower(v.profile);
    std::vector<std::string> args;

    if (v.codecName == "h264") {
        args = {"-c:v", "libx264", "-preset", s.preset, "-crf", std::to_string(s.crf)};
        const auto it = h264Profiles().find(profile);
        if (it != h264Profiles().end()) {
            args.push_back("-profile:v");
            args.push_back(it->second);
        }
        if (v.level > 0) {
            args.push_back("-level:v");
            args.push_back(std::to_string(v.level / 10) + "." + std::to_string(v.level % 10));
        }
        args.push_back("-x264-params");
        args.push_back("open-gop=0");
    } else if (v.codecName == "hevc") {
        args = {"-c:v", "libx265", "-preset", s.preset, "-crf", std::to_string(s.crf)};
        std::string x265 = "open-gop=0:repeat-headers=1:log-level=error";
        const auto it = h265Profiles().find(profile);
        if (it != h265Profiles().end()) x265 += ":profile=" + it->second;
        args.push_back("-x265-params");
        args.push_back(x265);
    } else {
        return std::nullopt;
    }

    if (!v.pixFmt.empty()) {
        args.push_back("-pix_fmt");
        args.push_back(v.pixFmt);
    }
    const std::pair<const std::string*, const char*> colors[] = {
        {&v.colorSpace, "-colorspace"},
        {&v.colorPrimaries, "-color_primaries"},
        {&v.colorTransfer, "-color_trc"},
        {&v.colorRange, "-color_range"},
    };
    for (const auto& [val, opt] : colors) {
        if (!val->empty() && *val != "unknown") {
            args.push_back(opt);
            args.push_back(*val);
        }
    }
    return args;
}

std::vector<std::string> fallbackEncoderArgs(const VideoInfo& v, const EncodeSettings& s) {
    return {"-c:v", "libx264", "-preset", s.preset, "-crf", std::to_string(s.crf),
            "-pix_fmt", v.pixFmt.empty() ? std::string("yuv420p") : v.pixFmt};
}

}  // namespace vidcut
