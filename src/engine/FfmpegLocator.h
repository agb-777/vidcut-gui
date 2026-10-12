#pragma once
#include <QString>
#include <QStringList>

namespace vidcut {

struct FfmpegTools {
    QString ffmpeg;   // full path
    QString ffprobe;  // full path
    bool isValid() const { return !ffmpeg.isEmpty() && !ffprobe.isEmpty(); }
};

// Finds ffmpeg + ffprobe on every OS. Search order:
//   1. user-chosen folder (File > Set FFmpeg Location...)
//   2. next to the application (and ./ffmpeg, ./bin, macOS Resources)
//   3. PATH
//   4. common macOS locations (Homebrew/MacPorts; Finder-launched apps get a minimal PATH)
class FfmpegLocator {
public:
    static FfmpegTools locate(const QString& customDir = QString());
    static FfmpegTools findInDir(const QString& dir);  // both tools must be in `dir`
};

}  // namespace vidcut
