#include "engine/DurationProbe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <string>

#include "core/NumParse.h"

namespace vidcut {

DurationProbe::DurationProbe(QObject* parent) : QObject(parent) {}

DurationProbe::~DurationProbe() { abort(); }

void DurationProbe::abort() {
    if (!m_proc) return;
    m_proc->disconnect(this);  // no more signals from the old process
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
    m_proc->deleteLater();
    m_proc = nullptr;
}

void DurationProbe::start(const QString& ffprobe, const QString& path) {
    abort();
    m_path = path;
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::finished, this, &DurationProbe::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, &DurationProbe::onErrorOccurred);
    m_proc->start(ffprobe, {QStringLiteral("-v"), QStringLiteral("error"),
                            QStringLiteral("-show_entries"),
                            QStringLiteral("format=duration:stream=codec_type"),
                            QStringLiteral("-of"), QStringLiteral("json"), path});
}

void DurationProbe::onErrorOccurred(QProcess::ProcessError error) {
    if (error != QProcess::FailedToStart) return;  // other errors end in finished()
    const QString path = m_path;
    const QString msg = QStringLiteral("Cannot start ffprobe: %1").arg(m_proc->errorString());
    m_proc->deleteLater();
    m_proc = nullptr;
    emit failed(path, msg);
}

void DurationProbe::onFinished(int exitCode, QProcess::ExitStatus status) {
    const QString path = m_path;
    const QByteArray out = m_proc->readAllStandardOutput();
    m_proc->deleteLater();
    m_proc = nullptr;

    if (status != QProcess::NormalExit || exitCode != 0) {
        emit failed(path, QStringLiteral("This file could not be read as a video."));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(out).object();
    bool hasVideo = false;
    const QJsonArray streams = root.value(QLatin1String("streams")).toArray();
    for (const QJsonValue& s : streams) {
        if (s.toObject().value(QLatin1String("codec_type")).toString() == QLatin1String("video"))
            hasVideo = true;
    }
    if (!hasVideo) {
        emit failed(path, QStringLiteral("This file does not contain a video stream."));
        return;
    }

    double duration = 0.0;
    const QJsonValue dv = root.value(QLatin1String("format")).toObject().value(QLatin1String("duration"));
    if (dv.isDouble()) duration = dv.toDouble();
    else if (dv.isString()) parseDouble(dv.toString().toStdString(), duration);
    if (duration <= 0.0) {
        emit failed(path, QStringLiteral("Could not read the duration of this video."));
        return;
    }
    emit succeeded(path, duration);
}

}  // namespace vidcut
