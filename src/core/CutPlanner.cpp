#include "core/CutPlanner.h"

#include <algorithm>

namespace vidcut {

std::optional<CutPlan> makePlan(const std::vector<Packet>& pkts, double start, double end) {
    std::vector<double> pts;
    pts.reserve(pkts.size());
    for (const auto& p : pkts) pts.push_back(p.pts);
    std::sort(pts.begin(), pts.end());
    if (pts.size() < 2) return std::nullopt;

    std::vector<double> diffs;
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        if (pts[i + 1] > pts[i]) diffs.push_back(pts[i + 1] - pts[i]);
    }
    std::sort(diffs.begin(), diffs.end());
    const double frameDur = diffs.empty() ? 0.04 : diffs[diffs.size() / 2];

    const auto sIt = std::find_if(pts.begin(), pts.end(), [&](double p) { return p >= start - kEps; });
    if (sIt == pts.end()) return std::nullopt;
    const double sEff = *sIt;

    const auto eIt = std::find_if(pts.begin(), pts.end(), [&](double p) { return p >= end - kEps; });
    const bool toEof = (eIt == pts.end());
    const double eEff = toEof ? pts.back() + frameDur : *eIt;
    if (eEff - sEff < frameDur * 0.5) return std::nullopt;

    const std::vector<std::size_t> closed = closedKeyframeIndices(pkts);
    if (closed.empty()) return std::nullopt;

    const auto i1It = std::find_if(closed.begin(), closed.end(),
                                   [&](std::size_t i) { return pkts[i].pts >= sEff - kEps; });
    if (i1It == closed.end()) return std::nullopt;
    const std::size_t i1 = *i1It;
    const double k1 = pkts[i1].pts;

    double k2 = 0.0, cLen = 0.0;
    std::optional<int> bPackets;
    if (toEof) {
        k2 = eEff;
    } else {
        const auto i2It = std::find_if(closed.rbegin(), closed.rend(),
                                       [&](std::size_t i) { return pkts[i].pts <= eEff + kEps; });
        if (i2It == closed.rend()) return std::nullopt;
        const std::size_t i2 = *i2It;
        if (pkts[i2].pts - k1 < frameDur * 1.5 || i2 <= i1) return std::nullopt;
        k2 = pkts[i2].pts;
        bPackets = static_cast<int>(i2 - i1);
        cLen = eEff - k2;
    }

    if (k1 >= eEff - kEps) return std::nullopt;

    CutPlan plan;
    plan.sEff = sEff;
    plan.eEff = eEff;
    plan.k1 = k1;
    plan.k2 = k2;
    plan.aLen = std::max(0.0, k1 - sEff);
    plan.bLen = k2 - k1;
    plan.cLen = std::max(0.0, cLen);
    plan.bPackets = bPackets;
    return plan;
}

}  // namespace vidcut
