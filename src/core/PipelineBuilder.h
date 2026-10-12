#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/CutPlanner.h"
#include "core/EncodeSettings.h"
#include "core/MediaInfo.h"

namespace vidcut {

struct PipelineParams {
    std::string input;      // absolute path of the source file
    std::string tmpDir;     // scratch directory (forward slashes)
    std::string outputExt;  // lower-case with dot: ".mp4" / ".mkv" / ".mov"
    MediaInfo media;
    std::vector<std::string> encoderArgs;  // from buildEncoderArgs / fallbackEncoderArgs
    std::optional<CutPlan> plan;           // nullopt = re-encode the whole range
    double start = 0.0;                    // requested range (seconds)
    double end = 0.0;
    EncodeSettings settings;
};

// One ffmpeg invocation. The runner prepends:  -hide_banner -loglevel error -y
struct FfmpegStep {
    std::string label;
    std::vector<std::string> args;
    double total = 0.0;       // media seconds produced (for the progress bar)
    std::string checkOutput;  // if set: must exist and be non-empty afterwards
};

struct Pipeline {
    std::vector<FfmpegStep> steps;     // executed in order; last one is the final mux
    std::string concatListPath;        // must be written before the mux step
    std::string concatListContent;
    std::string finalTmpPath;          // result of the mux step (to be moved to the output)
    double sEff = 0.0, eEff = 0.0;     // effective range, used for the "target" duration
};

Pipeline buildPipeline(const PipelineParams& p);

}  // namespace vidcut
