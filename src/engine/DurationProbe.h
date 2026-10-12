#pragma once
#include <QObject>
#include <QProcess>
#include <QString>

namespace vidcut {

// Reads the duration of a video in the background (does not block the UI).
// Starting a new probe aborts the previous one.
class DurationProbe : public QObject {
    Q_OBJECT
public:
    explicit DurationProbe(QObject* parent = nullptr);
    ~DurationProbe() override;

    void start(const QString& ffprobe, const QString& path);
    void abort();

signals:
    void succeeded(const QString& path, double durationSeconds);
    void failed(const QString& path, const QString& message);

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void onErrorOccurred(QProcess::ProcessError error);

private:
    QProcess* m_proc = nullptr;
    QString m_path;
};

}  // namespace vidcut
