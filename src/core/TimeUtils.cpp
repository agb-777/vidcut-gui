#include "core/TimeUtils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>

namespace vidcut {

std::string formatTime(double seconds) {
    const long long ms = std::llround(std::max(0.0, seconds) * 1000.0);
    const long long h = ms / 3600000;
    const long long m = (ms / 60000) % 60;
    const long long s = (ms / 1000) % 60;
    const long long milli = ms % 1000;
    char buf[64];
    std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld.%03lld", h, m, s, milli);
    return buf;
}

std::string formatFixed(double value, int decimals) {
    std::ostringstream os;
    os.imbue(std::locale::classic());
    os << std::fixed << std::setprecision(decimals) << value;
    return os.str();
}

std::string formatSeconds(double seconds) { return formatFixed(seconds, 6); }

std::string formatSize(std::uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    static const char* units[] = {"KB", "MB", "GB"};
    double n = static_cast<double>(bytes) / 1024.0;
    int u = 0;
    while (n >= 1024.0 && u < 2) {
        n /= 1024.0;
        ++u;
    }
    return formatFixed(n, 1) + " " + units[u];
}

}  // namespace vidcut
