// Unit tests for the Qt-free core library.  Run:  ctest   (or ./vidcut_core_tests)
#include <algorithm>
#include <cmath>
#include <iostream>

#include "core/CutPlanner.h"
#include "core/EncodeSettings.h"
#include "core/EncoderProfile.h"
#include "core/KeyframeAnalyzer.h"
#include "core/NumParse.h"
#include "core/PipelineBuilder.h"
#include "core/PlanFormatter.h"
#include "core/TimeUtils.h"

using namespace vidcut;

static int g_fail = 0, g_total = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_total;                                                               \
        if (!(cond)) {                                                           \
            ++g_fail;                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)
static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

static void testNumParse() {
    double d = 0;
    CHECK(parseDouble("0.033367", d) && near(d, 0.033367));
    CHECK(parseDouble("-0.066733", d) && near(d, -0.066733));
    CHECK(parseDouble("12", d) && near(d, 12));
    CHECK(parseDouble(" 1.5x" , d) == false);
    CHECK(parseDouble("1.5e2", d) && near(d, 150));
    CHECK(!parseDouble("N/A", d));
    CHECK(!parseDouble("", d));
    CHECK(!parseDouble(".", d));
    CHECK(parseDouble("5.", d) && near(d, 5));
}

static void testTime() {
    CHECK(parseTime("00:01:30") && near(*parseTime("00:01:30"), 90));
    CHECK(parseTime("01:00:00.5") && near(*parseTime("01:00:00.5"), 3600.5));
    CHECK(parseTime("  0:0:10 ") && near(*parseTime("0:0:10"), 10));
    CHECK(!parseTime("1:75:00"));
    CHECK(!parseTime("90"));
    CHECK(!parseTime("00:01"));
    CHECK(!parseTime("aa:bb:cc"));
    CHECK(!parseTime("99999999999999999999:00:00"));
    CHECK(formatTime(0) == "00:00:00.000");
    CHECK(formatTime(3661.5) == "01:01:01.500");
    CHECK(formatTime(119.9996) == "00:02:00.000");
    CHECK(formatTime(-5) == "00:00:00.000");
    CHECK(formatSeconds(1.5) == "1.500000");
    CHECK(formatSize(512) == "512 B");
    CHECK(formatSize(1536) == "1.5 KB");
    CHECK(formatSize(5ull * 1024 * 1024 * 1024) == "5.0 GB");
}

static void testSettings() {
    EncodeSettings s = defaultSettings();
    CHECK(s.preset == "ultrafast" && s.crf == 20 && s.audioBitrate == "192k");
    CHECK(validateSettings(s).empty());
    s.crf = 52; CHECK(!validateSettings(s).empty());
    s.crf = -1; CHECK(!validateSettings(s).empty());
    s.crf = 0; CHECK(validateSettings(s).empty());
    s.preset = "turbo"; CHECK(!validateSettings(s).empty());
    s.preset = "veryslow";
    for (const char* ok : {"96k", "320k", "1M", "192000", "128.5k"}) {
        s.audioBitrate = ok; CHECK(validateSettings(s).empty());
    }
    for (const char* bad : {"", "abc", "192kb", "-5k", "1k", "999M", "k"}) {
        s.audioBitrate = bad; CHECK(!validateSettings(s).empty());
    }
}

static void testEncoder() {
    VideoInfo v; v.codecName = "h264"; v.profile = "High"; v.level = 41; v.pixFmt = "yuv420p";
    v.colorSpace = "bt709"; v.colorRange = "tv"; v.colorTransfer = "unknown";
    EncodeSettings s; s.preset = "slow"; s.crf = 18;
    auto a = buildEncoderArgs(v, s);
    CHECK(a.has_value());
    auto has = [&](const std::string& x) { return std::find(a->begin(), a->end(), x) != a->end(); };
    CHECK(has("libx264") && has("slow") && has("18") && has("high") && has("4.1"));
    CHECK(has("open-gop=0") && has("bt709") && has("tv") && !has("unknown"));
    v.codecName = "hevc"; v.profile = "Main 10";
    a = buildEncoderArgs(v, s);
    CHECK(a && std::find(a->begin(), a->end(), "open-gop=0:repeat-headers=1:log-level=error:profile=main10") != a->end());
    v.codecName = "vp9";
    CHECK(!buildEncoderArgs(v, s).has_value());
    auto fb = fallbackEncoderArgs(v, s);
    CHECK(fb[1] == "libx264" && fb.back() == "yuv420p");
}

// 30 fps, GOP of 30 frames with B-frame reordering (decode order I P B B P B B ...)
static std::vector<Packet> makeGop(int frames, int gop, double fps) {
    std::vector<Packet> p;
    // I frame at the start of each GOP, then P/B groups of 3 in decode order (P before its 2 Bs)
    for (int g = 0; g * gop < frames; ++g) {
        const int b0 = g * gop;
        p.push_back({b0 / fps, true});  // I frame
        for (int i = 1; i < gop; i += 3) {
            const int pf = std::min(i + 2, gop - 1);  // relative to GOP start
            p.push_back({(b0 + pf) / fps, false});
            for (int j = i; j < pf; ++j) p.push_back({(b0 + j) / fps, false});
        }
    }
    return p;
}

static void testPlanner() {
    auto pk = makeGop(300, 30, 30.0);  // keyframes at 0,1,2,... s
    auto ks = closedKeyframeIndices(pk);
    CHECK(ks.size() == 10);

    auto plan = makePlan(pk, 2.5, 7.5);
    CHECK(plan.has_value());
    if (plan) {
        CHECK(near(plan->sEff, 2.5, 0.0006) && near(plan->k1, 3.0, 1e-6));
        CHECK(near(plan->k2, 7.0, 1e-6) && near(plan->eEff, 7.5, 0.0006));
        CHECK(plan->aLen > kEps && plan->cLen > kEps);
        CHECK(plan->bPackets && *plan->bPackets == 4 * 30);
        auto rows = describePlan(plan, 2.5, 7.5);
        CHECK(rows.size() == 3 && rows[1].mode == "Copy");
    }
    // exactly on keyframes -> no A / C segments
    auto exact = makePlan(pk, 3.0, 6.0);
    CHECK(exact && exact->aLen <= kEps);
    // too short: no closed keyframe inside -> nullopt
    CHECK(!makePlan(pk, 3.1, 3.5).has_value());
    // to EOF
    auto eof = makePlan(pk, 4.0, 1000.0);
    CHECK(eof && !eof->bPackets && eof->cLen == 0.0);
    CHECK(!makePlan({}, 0, 1).has_value());
    CHECK(describePlan(std::nullopt, 1, 2).size() == 1);
}

static void testPackets() {
    auto r = parsePackets("0.000000,K_\r\n0.066667,__\n\n0.033333,__\n", 0.0);
    CHECK(r && r->size() == 3 && (*r)[0].keyframe && !(*r)[1].keyframe);
    CHECK(!parsePackets("N/A,K_\n", 0).has_value());
    CHECK(!parsePackets("", 0).has_value());
    auto s = parsePackets("1.5,K_\n", 1.0);
    CHECK(s && near((*s)[0].pts, 0.5));
}

static void testPipeline() {
    PipelineParams pp;
    pp.input = "/in/video's.mp4"; pp.tmpDir = "/out/.t"; pp.outputExt = ".mp4";
    pp.media.video.codecName = "hevc"; pp.media.video.index = 0; pp.media.video.rotation = -90;
    pp.media.hasAudio = true; pp.media.duration = 10;
    pp.encoderArgs = {"-c:v", "libx265"};
    pp.start = 2.5; pp.end = 7.5;
    pp.plan = makePlan(makeGop(300, 30, 30.0), 2.5, 7.5);
    auto pl = buildPipeline(pp);
    CHECK(pl.steps.size() == 5);  // A, B, C, audio, mux
    CHECK(pl.concatListContent.find("video") == std::string::npos);
    CHECK(pl.concatListContent.find("duration") != std::string::npos);
    const auto& mux = pl.steps.back().args;
    CHECK(std::find(mux.begin(), mux.end(), "hvc1") != mux.end());
    CHECK(mux[0] == "-display_rotation" && mux[1] == "-90");
    // no plan -> full re-encode
    pp.plan.reset();
    pl = buildPipeline(pp);
    CHECK(pl.steps.size() == 3);
    pp.media.hasAudio = false; pp.outputExt = ".mkv";
    pl = buildPipeline(pp);
    CHECK(pl.steps.size() == 2);
    CHECK(std::find(pl.steps.back().args.begin(), pl.steps.back().args.end(), "+faststart") == pl.steps.back().args.end());
}

int main() {
    testNumParse(); testTime(); testSettings(); testEncoder();
    testPlanner(); testPackets(); testPipeline();
    std::cout << (g_total - g_fail) << "/" << g_total << " checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
