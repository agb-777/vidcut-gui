#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>

#include "engine/CutErrors.h"

namespace vidcut {

// Blocking process helper meant to run inside the worker thread.
// All functions throw CutError on failure and CutCancelled when cancelled.
class ProcessRunner {
public:
    // currentSeconds = media time produced so far, speed = ffmpeg "speed" (0 if unknown)
    using ProgressFn = std::function<void(double currentSeconds, double speed)>;

    explicit ProcessRunner(const CancelToken& token) : m_cancel(token) {}

    // Runs a program to completion and returns its stdout.
    QByteArray capture(const QString& program, const QStringList& args);

    // Runs ffmpeg with "-progress pipe:1" and reports progress.
    // `args` are everything after "-hide_banner -loglevel error -y".
    void runFfmpeg(const QString& ffmpeg, const QStringList& args, double totalSeconds,
                   const ProgressFn& onProgress);

private:
    const CancelToken& m_cancel;
};

}  // namespace vidcut
