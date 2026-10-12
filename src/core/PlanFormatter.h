#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/CutPlanner.h"

namespace vidcut {

struct PlanRow {
    std::string segment;   // "A", "B", "C" or "All"
    std::string mode;      // "Re-encode" / "Copy"
    std::string range;     // "HH:MM:SS.mmm - HH:MM:SS.mmm"
    std::string duration;  // "1.234s"
};

// Rows for the plan table shown in the UI. If plan is nullopt, a single
// "re-encode everything" row for [start, end] is returned.
std::vector<PlanRow> describePlan(const std::optional<CutPlan>& plan, double start, double end);

}  // namespace vidcut
