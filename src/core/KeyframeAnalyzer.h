#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace vidcut {

struct Packet {
    double pts = 0.0;      // seconds, already shifted by the container start time
    bool keyframe = false;
};

// Parses `ffprobe -show_entries packet=pts_time,flags -of csv=p=0` output.
// Returns nullopt if the output is empty or any timestamp is missing/invalid.
std::optional<std::vector<Packet>> parsePackets(const std::string& csv, double startOffset);

// Indices of keyframes that are "closed": no packet before it (in decode order)
// has a later PTS, and no packet after it has an earlier PTS. Only such
// keyframes are safe cut points for stream copy.
std::vector<std::size_t> closedKeyframeIndices(const std::vector<Packet>& pkts);

}  // namespace vidcut
