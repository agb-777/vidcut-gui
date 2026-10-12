#pragma once
#include <QString>
#include <optional>
#include <vector>

#include "core/KeyframeAnalyzer.h"
#include "core/MediaInfo.h"
#include "engine/ProcessRunner.h"

namespace vidcut {

// ffprobe wrappers.
class MediaProbe {
public:
    // Stream/format info of the first real video stream (cover art is ignored).
    static MediaInfo probe(ProcessRunner& runner, const QString& ffprobe, const QString& path);

    // Video packet timestamps + keyframe flags. nullopt if incomplete.
    static std::optional<std::vector<Packet>> readPackets(ProcessRunner& runner,
                                                          const QString& ffprobe,
                                                          const QString& path, int streamIndex,
                                                          double startOffset);

    // Container duration of a finished file; <= 0 if unknown.
    static double readDuration(ProcessRunner& runner, const QString& ffprobe, const QString& path);
};

}  // namespace vidcut
