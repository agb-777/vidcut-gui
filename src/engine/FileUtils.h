#pragma once
#include <QString>

namespace vidcut {

// Moves `src` over `dst` (replacing it). Falls back to copy+delete if a plain
// rename is impossible. Returns false and fills `error` on failure.
bool replaceFile(const QString& src, const QString& dst, QString* error);

}  // namespace vidcut
