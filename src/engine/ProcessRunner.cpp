#include "engine/ProcessRunner.h"

#include <QProcess>
#include <algorithm>
#include <string>

#include "core/NumParse.h"

namespace vidcut {

namespace {

std::string toStd(const QByteArray& b) { return std::string(b.constData(), static_cast<std::size_t>(b.size())); }

QString describeFailure(const QString& program, const QStringList& args, const QByteArray& err,
                        const QString& reason) {
    QStringList lines = QString::fromUtf8(err).trimmed().split(QLatin1Char('\n'));
    if (lines.size() > 15) lines = lines.mid(lines.size() - 15);
    QString msg = QStringLiteral("Command failed: %1 %2").arg(program, args.join(QLatin1Char(' ')));
    if (!reason.isEmpty()) msg += QStringLiteral("\n") + reason;
    const QString tail = lines.join(QLatin1Char('\n')).trimmed();
    if (!tail.isEmpty()) msg += QStringLiteral("\n") + tail;
    return msg;
}

void startOrThrow(QProcess& proc, const QString& program, const QStringList& args) {
    proc.start(program, args);
    if (!proc.waitForStarted(15000))
        throw CutError(QStringLiteral("Cannot start '%1': %2").arg(program, proc.errorString()));
}

void killAndThrow(QProcess& proc) {
    proc.kill();
    proc.waitForFinished(5000);
    throw CutCancelled();
}

}  // namespace

QByteArray ProcessRunner::capture(const QString& program, const QStringList& args) {
    QProcess proc;
    startOrThrow(proc, program, args);

    QByteArray out, err;
    for (;;) {
        const bool finished = proc.waitForFinished(100);
        out += proc.readAllStandardOutput();  // drain both pipes so the child never blocks
        err += proc.readAllStandardError();
        if (finished || proc.state() == QProcess::NotRunning) break;
        if (m_cancel.isCancelled()) killAndThrow(proc);
    }
    out += proc.readAllStandardOutput();
    err += proc.readAllStandardError();

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        throw CutError(describeFailure(program, args, err, QString()));
    return out;
}

void ProcessRunner::runFfmpeg(const QString& ffmpeg, const QStringList& args, double totalSeconds,
                              const ProgressFn& onProgress) {
    QStringList full = {QStringLiteral("-nostdin"),   QStringLiteral("-progress"),
                        QStringLiteral("pipe:1"),     QStringLiteral("-nostats"),
                        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                        QStringLiteral("error"),      QStringLiteral("-y")};
    full += args;

    QProcess proc;
    startOrThrow(proc, ffmpeg, full);

    QByteArray outBuf, errAll;
    double current = 0.0, speed = 0.0;

    auto handleLine = [&](const QByteArray& raw) {
        const QByteArray line = raw.trimmed();
        const int eq = line.indexOf('=');
        if (eq < 0) return;
        const QByteArray key = line.left(eq);
        const QByteArray val = line.mid(eq + 1).trimmed();
        if (key == "out_time_us" || key == "out_time_ms") {  // both are microseconds in ffmpeg
            double us = 0.0;
            if (!parseDouble(toStd(val), us)) return;  // "N/A" early on
            current = std::max(0.0, us / 1e6);
            if (onProgress) onProgress(std::min(totalSeconds, current), speed);
        } else if (key == "speed") {
            QByteArray s = val;
            if (s.endsWith('x')) s.chop(1);
            double x = 0.0;
            speed = parseDouble(toStd(s.trimmed()), x) ? x : 0.0;
        } else if (key == "progress" && val == "end") {
            if (onProgress) onProgress(totalSeconds, speed);
        }
    };
    auto drain = [&]() {
        outBuf += proc.readAllStandardOutput();
        errAll += proc.readAllStandardError();
        int nl;
        while ((nl = outBuf.indexOf('\n')) >= 0) {
            handleLine(outBuf.left(nl));
            outBuf.remove(0, nl + 1);
        }
    };

    for (;;) {
        const bool finished = proc.waitForFinished(100);
        drain();
        if (finished || proc.state() == QProcess::NotRunning) break;
        if (m_cancel.isCancelled()) killAndThrow(proc);
    }
    drain();
    if (!outBuf.isEmpty()) handleLine(outBuf);

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        throw CutError(describeFailure(ffmpeg, full, errAll, QString()));
}

}  // namespace vidcut
