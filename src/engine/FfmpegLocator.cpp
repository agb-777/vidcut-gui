#include "engine/FfmpegLocator.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <utility>

namespace vidcut {

FfmpegTools FfmpegLocator::findInDir(const QString& dir) {
    FfmpegTools t;
    if (dir.isEmpty()) return t;
    const QStringList where{dir};
    const QString ff = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"), where);
    const QString fp = QStandardPaths::findExecutable(QStringLiteral("ffprobe"), where);
    if (!ff.isEmpty() && !fp.isEmpty()) {
        t.ffmpeg = ff;
        t.ffprobe = fp;
    }
    return t;
}

FfmpegTools FfmpegLocator::locate(const QString& customDir) {
    QStringList dirs;
    if (!customDir.isEmpty()) dirs << customDir;

    const QString app = QCoreApplication::applicationDirPath();
    dirs << app << app + QStringLiteral("/ffmpeg") << app + QStringLiteral("/bin");
#ifdef Q_OS_MACOS
    dirs << app + QStringLiteral("/../Resources") << app + QStringLiteral("/../Resources/ffmpeg");
#endif

    const QString pathEnv = QString::fromLocal8Bit(qgetenv("PATH"));
    dirs << pathEnv.split(QDir::listSeparator(), Qt::SkipEmptyParts);

#ifdef Q_OS_MACOS
    dirs << QStringLiteral("/opt/homebrew/bin") << QStringLiteral("/usr/local/bin")
         << QStringLiteral("/opt/local/bin");
#endif

    for (const QString& d : std::as_const(dirs)) {
        const FfmpegTools t = findInDir(QDir::cleanPath(d));
        if (t.isValid()) return t;
    }

    // Last resort: tools living in different folders on PATH.
    FfmpegTools t;
    t.ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    t.ffprobe = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    if (!t.isValid()) return FfmpegTools{};
    return t;
}

}  // namespace vidcut
