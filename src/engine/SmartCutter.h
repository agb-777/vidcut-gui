#pragma once
#include <QObject>
#include <QString>

#include "core/EncodeSettings.h"
#include "engine/CutErrors.h"

namespace vidcut {

struct CutJob {
    QString input;   // absolute path
    QString output;  // absolute path (.mp4 / .mkv / .mov)
    double start = 0.0;
    double end = 0.0;
    EncodeSettings settings;
    QString ffmpeg;   // full path to executables
    QString ffprobe;
};

// Runs the whole smartcut pipeline. Create it WITHOUT a parent, move it to a
// QThread and connect QThread::started -> run().  All signals are emitted from
// the worker thread (Qt queues them to the UI thread automatically).
class SmartCutter : public QObject {
    Q_OBJECT
public:
    explicit SmartCutter(const CutJob& job, QObject* parent = nullptr);

    // Thread-safe; may be called from the UI thread at any time.
    void requestCancel();

public slots:
    void run();

signals:
    void stageChanged(const QString& label, int index, int count);
    void progress(double fraction, double speed, double etaSeconds);  // eta < 0 = unknown
    void succeeded(const QString& summary);
    void failed(const QString& message);
    void cancelled();
    void finishedRunning();  // always emitted last

private:
    void execute();

    CutJob m_job;
    CancelToken m_cancel;
};

}  // namespace vidcut
