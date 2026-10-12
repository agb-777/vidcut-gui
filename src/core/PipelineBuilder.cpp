#include "core/PipelineBuilder.h"

#include <algorithm>
#include <utility>

#include "core/TimeUtils.h"

namespace vidcut {

namespace {

void append(std::vector<std::string>& dst, const std::vector<std::string>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

std::string escapeConcatPath(const std::string& path) {
    std::string out;
    for (char c : path) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    return out;
}

}  // namespace

Pipeline buildPipeline(const PipelineParams& p) {
    Pipeline pl;
    const VideoInfo& v = p.media.video;
    const std::string vmap = "0:" + std::to_string(v.index);
    const std::string& codec = v.codecName;
    const auto sec = formatSeconds;
    auto tmp = [&](const std::string& name) { return p.tmpDir + "/" + name; };

    // (path, duration) pairs for the concat list; duration < 0 means "not written".
    std::vector<std::pair<std::string, double>> segments;
    double sEff = 0.0, eEff = 0.0;

    if (!p.plan) {
        // ---- whole range re-encoded --------------------------------------
        sEff = p.start;
        eEff = (p.media.duration > 0.0) ? std::min(p.end, p.media.duration) : p.end;
        const std::string ts = tmp("full.ts");
        const double seek = std::max(0.0, sEff - kEps);
        const double relEnd = eEff - seek;

        std::vector<std::string> a = {"-noautorotate", "-ss", sec(seek), "-i", p.input,
                                      "-t", sec(relEnd + 1), "-vf", "trim=end=" + sec(relEnd),
                                      "-map", vmap, "-an"};
        append(a, p.encoderArgs);
        append(a, {"-fps_mode", "passthrough", "-f", "mpegts", ts});
        pl.steps.push_back({"Re-encoding the whole range...", a, eEff - sEff, ts});
        segments.push_back({ts, -1.0});
    } else {
        // ---- smartcut: A (re-encode) + B (copy) + C (re-encode) ----------
        const CutPlan& c = *p.plan;
        sEff = c.sEff;
        eEff = c.eEff;

        if (c.aLen > kEps) {
            const std::string ts = tmp("a.ts");
            std::vector<std::string> a = {"-noautorotate", "-ss", sec(std::max(0.0, sEff - kEps)),
                                          "-i", p.input, "-t", sec(c.aLen),
                                          "-map", vmap, "-an"};
            append(a, p.encoderArgs);
            append(a, {"-fps_mode", "passthrough", "-f", "mpegts", ts});
            pl.steps.push_back({"[A] Re-encoding " + formatFixed(c.aLen, 3) +
                                    "s (start to first keyframe)...",
                                a, c.aLen, ts});
            segments.push_back({ts, c.aLen});
        }

        {
            const std::string ts = tmp("b.ts");
            std::vector<std::string> a = {"-ss", sec(c.k1 + kEps), "-i", p.input,
                                          "-map", vmap, "-an"};
            if (c.bPackets) append(a, {"-frames:v", std::to_string(*c.bPackets)});
            append(a, {"-c:v", "copy"});
            if (codec == "h264") append(a, {"-bsf:v", "h264_mp4toannexb"});
            else if (codec == "hevc") append(a, {"-bsf:v", "hevc_mp4toannexb"});
            append(a, {"-avoid_negative_ts", "make_zero", "-f", "mpegts", ts});
            pl.steps.push_back({"[B] Copying " + formatFixed(c.bLen, 3) + "s without re-encode...",
                                a, c.bLen, ts});
            segments.push_back({ts, c.bLen});
        }

        if (c.cLen > kEps) {
            const std::string ts = tmp("c.ts");
            std::vector<std::string> a = {"-noautorotate", "-ss", sec(std::max(0.0, c.k2 - kEps)),
                                          "-i", p.input, "-t", sec(c.cLen),
                                          "-map", vmap, "-an"};
            append(a, p.encoderArgs);
            append(a, {"-fps_mode", "passthrough", "-f", "mpegts", ts});
            pl.steps.push_back({"[C] Re-encoding " + formatFixed(c.cLen, 3) +
                                    "s (last keyframe to end)...",
                                a, c.cLen, ts});
            segments.push_back({ts, c.cLen});
        }
    }

    // ---- concat list -----------------------------------------------------
    pl.concatListPath = tmp("list.txt");
    for (const auto& [path, dur] : segments) {
        pl.concatListContent += "file '" + escapeConcatPath(path) + "'\n";
        if (dur >= 0.0) pl.concatListContent += "duration " + formatSeconds(dur) + "\n";
    }

    // ---- audio (always re-encoded to AAC) -----------------------------------
    std::string audioPath;
    if (p.media.hasAudio) {
        audioPath = tmp("audio.mkv");
        pl.steps.push_back({"Re-encoding audio (AAC)...",
                            {"-ss", sec(std::max(0.0, sEff)), "-i", p.input,
                             "-t", sec(eEff - sEff), "-map", "0:a", "-vn", "-sn",
                             "-c:a", "aac", "-b:a", p.settings.audioBitrate, audioPath},
                            eEff - sEff, ""});
    }

    // ---- final mux -------------------------------------------------------
    std::vector<std::string> mux;
    if (v.rotation != 0) append(mux, {"-display_rotation", std::to_string(v.rotation)});
    append(mux, {"-f", "concat", "-safe", "0", "-i", pl.concatListPath});
    if (!audioPath.empty()) append(mux, {"-i", audioPath});
    append(mux, {"-map", "0:v:0"});
    if (!audioPath.empty()) append(mux, {"-map", "1:a"});
    append(mux, {"-c", "copy"});
    if (p.outputExt == ".mp4" || p.outputExt == ".mov") {
        if (codec == "hevc") append(mux, {"-tag:v", "hvc1"});
        append(mux, {"-movflags", "+faststart"});
    }
    pl.finalTmpPath = tmp("output" + p.outputExt);
    append(mux, {"-avoid_negative_ts", "make_zero", pl.finalTmpPath});
    pl.steps.push_back({"Merging all segments...", mux, eEff - sEff, ""});

    pl.sEff = sEff;
    pl.eEff = eEff;
    return pl;
}

}  // namespace vidcut
