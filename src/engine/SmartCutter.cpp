#include "engine/SmartCutter.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QTemporaryDir>
#include <algorithm>
#include <cstdint>
#include <optional>

#include "core/CutPlanner.h"
#include "core/EncoderProfile.h"
#include "core/PipelineBuilder.h"
#include "core/PlanFormatter.h"
#include "core/TimeUtils.h"
#include "engine/FileUtils.h"
#include "engine/MediaProbe.h"
#include "engine/ProcessRunner.h"

namespace vidcut {

namespace {
QString q(const std::string& s) { return QString::fromStdString(s); }
}  // namespace

SmartCutter::SmartCutter(const CutJob& job, QObject* parent) : QObject(parent), m_job(job) {}

void SmartCutter::requestCancel() { m_cancel.cancel(); }

void SmartCutter::run() {
    try {
        execute();
    } catch (const CutCancelled&) {
        emit cancelled();
    } catch (const CutError& e) {
        emit failed(QString::fromUtf8(e.what()));
    } catch (const std::exception& e) {
        emit failed(QStringLiteral("Unexpected error: %1").arg(QString::fromUtf8(e.what())));
    }
    emit finishedRunning();
}

void SmartCutter::execute() {
    ProcessRunner runner(m_cancel);
    auto info = [this](const QString& msg) { emit log(QStringLiteral("INFO  ") + msg); };

    // ---- 1. ffmpeg capability check (same rules as the original script) -----
    const QByteArray optRaw =
        runner.capture(m_job.ffmpeg, {QStringLiteral("-hide_banner"), QStringLiteral("-h"),
                                      QStringLiteral("long")});
    const bool optionsKnown = optRaw.count('\n') > 20;
    const QString optText = QString::fromUtf8(optRaw);
    auto hasOption = [&](const QString& name) {
        if (!optionsKnown) return true;
        const QRegularExpression re(QStringLiteral("^\\s*-") + name + QStringLiteral("\\b"),
                                    QRegularExpression::MultilineOption);
        return re.match(optText).hasMatch();
    };
    if (!hasOption(QStringLiteral("fps_mode")))
        throw CutError(QStringLiteral(
            "This ffmpeg is too old (option -fps_mode is missing). Please upgrade ffmpeg."));

    // ---- 2. probe -----------------------------------------------------------
    info(QStringLiteral("Analyzing input file..."));
    const MediaInfo media = MediaProbe::probe(runner, m_job.ffprobe, m_job.input);
    const VideoInfo& v = media.video;

    if (v.rotation != 0 && !hasOption(QStringLiteral("display_rotation")))
        throw CutError(QStringLiteral(
            "This video is rotated, which requires a newer ffmpeg "
            "(option -display_rotation is missing). Please upgrade ffmpeg."));

    if (media.duration > 0 && m_job.start >= media.duration)
        throw CutError(QStringLiteral("Start (%1) is beyond the video duration (%2).")
                           .arg(q(formatTime(m_job.start)), q(formatTime(media.duration))));
    if (media.duration > 0 && m_job.end > media.duration)
        info(QStringLiteral("End exceeds video duration, clamped to %1")
                 .arg(q(formatTime(media.duration))));

    // ---- 3. encoder + plan --------------------------------------------------
    const auto smartEnc = buildEncoderArgs(v, m_job.settings);
    bool smartOk = smartEnc.has_value();
    std::vector<std::string> encArgs;
    if (smartOk) {
        encArgs = *smartEnc;
    } else {
        info(QStringLiteral("Codec '%1' is not supported by smartcut, falling back to full "
                            "re-encode (libx264)")
                 .arg(q(v.codecName)));
        encArgs = fallbackEncoderArgs(v, m_job.settings);
    }

    std::optional<CutPlan> plan;
    if (smartOk) {
        info(QStringLiteral("Reading keyframes..."));
        const auto pkts =
            MediaProbe::readPackets(runner, m_job.ffprobe, m_job.input, v.index, media.startTime);
        if (!pkts) {
            smartOk = false;
            info(QStringLiteral("Incomplete packet timestamps, falling back to full re-encode"));
        } else {
            plan = makePlan(*pkts, m_job.start, m_job.end);
            if (!plan)
                info(QStringLiteral("Range too short or no suitable closed keyframe, "
                                    "re-encoding this range only"));
        }
    }

    QVariantList rows;
    for (const PlanRow& r : describePlan(plan, m_job.start, m_job.end))
        rows << QVariant(QStringList{q(r.segment), q(r.mode), q(r.range), q(r.duration)});
    emit planReady(rows);

    // ---- 4. scratch directory next to the output (RAII: removed on any exit) --
    const QString outDir = QFileInfo(m_job.output).absolutePath();
    QTemporaryDir tmp(QDir(outDir).filePath(QStringLiteral(".vidcut_XXXXXX")));
    if (!tmp.isValid())
        throw CutError(QStringLiteral("Cannot write to output directory: %1").arg(tmp.errorString()));

    PipelineParams pp;
    pp.input = m_job.input.toStdString();
    pp.tmpDir = QDir::fromNativeSeparators(tmp.path()).toStdString();
    pp.outputExt = (QStringLiteral(".") + QFileInfo(m_job.output).suffix().toLower()).toStdString();
    pp.media = media;
    pp.encoderArgs = encArgs;
    pp.plan = plan;
    pp.start = m_job.start;
    pp.end = m_job.end;
    pp.settings = m_job.settings;
    const Pipeline pipeline = buildPipeline(pp);

    {
        QFile list(q(pipeline.concatListPath));
        if (!list.open(QIODevice::WriteOnly) ||
            list.write(pipeline.concatListContent.data(), static_cast<qint64>(pipeline.concatListContent.size())) < 0)
            throw CutError(QStringLiteral("Cannot write temporary file: %1").arg(list.errorString()));
    }

    // ---- 5. run every step -----------------------------------------------------
    QElapsedTimer timer;
    timer.start();
    const int count = static_cast<int>(pipeline.steps.size());
    for (int i = 0; i < count; ++i) {
        const FfmpegStep& st = pipeline.steps[static_cast<std::size_t>(i)];
        emit stageChanged(q(st.label), i + 1, count);
        emit log(QStringLiteral(">>    ") + q(st.label));

        QStringList args;
        for (const std::string& a : st.args) args << q(a);

        const double total = st.total;
        runner.runFfmpeg(m_job.ffmpeg, args, total, [this, total](double cur, double speed) {
            const double frac = total > 0 ? std::min(1.0, cur / total) : 0.0;
            const double eta = (speed > 0 && total > 0) ? std::max(0.0, (total - cur) / speed) : -1.0;
            emit progress(frac, speed, eta);
        });

        if (!st.checkOutput.empty()) {
            const QFileInfo fi(q(st.checkOutput));
            if (!fi.exists() || fi.size() == 0)
                throw CutError(
                    QStringLiteral("ffmpeg produced an empty segment (%1). Seeking in this input is "
                                   "probably unreliable (common with .ts/mpegts). Remux it first, "
                                   "e.g.: ffmpeg -i input.ts -c copy input.mkv")
                        .arg(fi.fileName()));
        }
    }

    // ---- 6. move result into place --------------------------------------------
    QString err;
    if (!replaceFile(q(pipeline.finalTmpPath), m_job.output, &err)) throw CutError(err);

    const double outDur = MediaProbe::readDuration(runner, m_job.ffprobe, m_job.output);
    const double target = pipeline.eEff - pipeline.sEff;
    const qint64 size = QFileInfo(m_job.output).size();
    const double elapsed = timer.elapsed() / 1000.0;

    QString summary = QStringLiteral("Output:    %1\n").arg(QDir::toNativeSeparators(m_job.output));
    if (outDur > 0)
        summary += QStringLiteral("Duration:  %1 (target %2)\n")
                       .arg(q(formatTime(outDur)), q(formatTime(target)));
    summary += QStringLiteral("Size:      %1\nTime:      %2s")
                   .arg(q(formatSize(static_cast<std::uint64_t>(size))), q(formatFixed(elapsed, 1)));
    emit succeeded(summary);
}

}  // namespace vidcut
