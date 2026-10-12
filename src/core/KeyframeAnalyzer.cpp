#include "core/KeyframeAnalyzer.h"

#include <algorithm>
#include <limits>

#include "core/NumParse.h"

namespace vidcut {

std::optional<std::vector<Packet>> parsePackets(const std::string& csv, double startOffset) {
    std::vector<Packet> pkts;
    std::size_t pos = 0;
    while (pos <= csv.size()) {
        std::size_t nl = csv.find('\n', pos);
        if (nl == std::string::npos) nl = csv.size();
        std::string line = csv.substr(pos, nl - pos);
        pos = nl + 1;

        // strip whitespace (also handles "\r\n" from Windows ffprobe)
        const auto b = line.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) continue;
        const auto e = line.find_last_not_of(" \t\r\n");
        line = line.substr(b, e - b + 1);

        const auto comma = line.find(',');
        if (comma == std::string::npos) continue;  // fewer than 2 fields
        std::string flagsField = line.substr(comma + 1);
        const auto comma2 = flagsField.find(',');
        if (comma2 != std::string::npos) flagsField.resize(comma2);

        double pts = 0.0;
        if (!parseDouble(line.substr(0, comma), pts)) return std::nullopt;
        pkts.push_back({pts - startOffset, flagsField.find('K') != std::string::npos});
    }
    if (pkts.empty()) return std::nullopt;
    return pkts;
}

std::vector<std::size_t> closedKeyframeIndices(const std::vector<Packet>& pkts) {
    const std::size_t n = pkts.size();
    constexpr double inf = std::numeric_limits<double>::infinity();

    std::vector<double> prefixMax(n, -inf);
    double m = -inf;
    for (std::size_t i = 0; i < n; ++i) {
        prefixMax[i] = m;
        m = std::max(m, pkts[i].pts);
    }
    std::vector<double> suffixMin(n, inf);
    m = inf;
    for (std::size_t i = n; i-- > 0;) {
        suffixMin[i] = m;
        m = std::min(m, pkts[i].pts);
    }

    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < n; ++i) {
        if (pkts[i].keyframe && prefixMax[i] < pkts[i].pts && pkts[i].pts < suffixMin[i])
            out.push_back(i);
    }
    return out;
}

}  // namespace vidcut
