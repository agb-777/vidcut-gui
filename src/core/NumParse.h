#pragma once
#include <string>

namespace vidcut {

// Locale-independent string -> double. Accepts [+-]digits[.digits][e[+-]digits].
// Returns false for anything else (e.g. "N/A", "", "12abc").
bool parseDouble(const std::string& text, double& out);

}  // namespace vidcut
