#pragma once
#include <string>

namespace vidcut {

// Properties of the first real video stream (filled from ffprobe output).
struct VideoInfo {
    int index = 0;
    std::string codecName;
    std::string profile;  // as reported by ffprobe, e.g. "High"
    std::string pixFmt;
    int level = 0;
    std::string colorSpace, colorPrimaries, colorTransfer, colorRange;
    int rotation = 0;  // degrees, from display matrix / rotate tag
};

struct MediaInfo {
    VideoInfo video;
    bool hasAudio = false;
    double duration = 0.0;
    double startTime = 0.0;
};

}  // namespace vidcut
