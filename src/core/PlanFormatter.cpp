#include "core/PlanFormatter.h"

#include "core/TimeUtils.h"

namespace vidcut {

static std::string range(double a, double b) { return formatTime(a) + " - " + formatTime(b); }
static std::string dur(double d) { return formatFixed(d, 3) + "s"; }

std::vector<PlanRow> describePlan(const std::optional<CutPlan>& plan, double start, double end) {
    std::vector<PlanRow> rows;
    if (!plan) {
        rows.push_back({"All", "Re-encode", range(start, end), dur(end - start)});
        return rows;
    }
    if (plan->aLen > kEps)
        rows.push_back({"A", "Re-encode", range(plan->sEff, plan->k1), dur(plan->aLen)});
    rows.push_back({"B", "Copy", range(plan->k1, plan->k2), dur(plan->bLen)});
    if (plan->cLen > kEps)
        rows.push_back({"C", "Re-encode", range(plan->k2, plan->eEff), dur(plan->cLen)});
    return rows;
}

}  // namespace vidcut
