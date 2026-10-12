#include "core/NumParse.h"

#include <cctype>
#include <cmath>

namespace vidcut {

bool parseDouble(const std::string& text, double& out) {
    size_t i = 0, n = text.size();
    while (i < n && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    while (n > i && std::isspace(static_cast<unsigned char>(text[n - 1]))) --n;
    if (i >= n) return false;

    bool neg = false;
    if (text[i] == '+' || text[i] == '-') {
        neg = (text[i] == '-');
        ++i;
    }

    unsigned long long mant = 0;
    int sigDigits = 0;
    int exp10 = 0;
    bool any = false;
    bool inFraction = false;

    for (; i < n; ++i) {
        const char c = text[i];
        if (c == '.' && !inFraction) {
            inFraction = true;
            continue;
        }
        if (c < '0' || c > '9') break;
        any = true;
        const int d = c - '0';
        if (mant == 0 && d == 0) {
            if (inFraction) --exp10;
        } else if (sigDigits < 18) {
            mant = mant * 10 + static_cast<unsigned>(d);
            ++sigDigits;
            if (inFraction) --exp10;
        } else if (!inFraction) {
            ++exp10;  // digit dropped from integer part
        }
    }
    if (!any) return false;

    if (i < n && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool eneg = false;
        if (i < n && (text[i] == '+' || text[i] == '-')) {
            eneg = (text[i] == '-');
            ++i;
        }
        if (i >= n) return false;
        int e = 0;
        for (; i < n && text[i] >= '0' && text[i] <= '9'; ++i) {
            if (e < 10000) e = e * 10 + (text[i] - '0');
        }
        exp10 += eneg ? -e : e;
    }
    if (i != n) return false;

    long double v = static_cast<long double>(mant);
    if (exp10 < 0) v /= std::pow(10.0L, -exp10);
    else if (exp10 > 0) v *= std::pow(10.0L, exp10);
    out = static_cast<double>(neg ? -v : v);
    return true;
}

}  // namespace vidcut
