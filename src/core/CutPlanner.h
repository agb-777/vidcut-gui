#pragma once
#include <optional>
#include <vector>

#include "core/KeyframeAnalyzer.h"

namespace vidcut {

constexpr double kEps = 0.0005;

// Smartcut plan:  [A: re-encode] [B: stream copy] [C: re-encode]
//   A = sEff .. k1 (start to first closed keyframe)
//   B = k1 .. k2   (copied untouched)
//   C = k2 .. eEff (last closed keyframe to end)
struct CutPlan {
    double sEff = 0, eEff = 0;
    double k1 = 0, k2 = 0;
    double aLen = 0, bLen = 0, cLen = 0;
    std::optional<int> bPackets;  // number of video packets in B (nullopt = until EOF)
};

// Returns nullopt when the range is too short or has no usable closed keyframe;
// the caller should then re-encode the whole range.
std::optional<CutPlan> makePlan(const std::vector<Packet>& pkts, double start, double end);

}  // namespace vidcut
