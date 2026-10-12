#pragma once
#include <QByteArray>
#include <QSettings>
#include <QString>

#include "core/EncodeSettings.h"

namespace vidcut {

// Persistent user settings (QSettings: registry on Windows, plist on macOS, ini on Linux).
// QCoreApplication organization/application names must be set before use.
class SettingsStore {
public:
    SettingsStore() = default;

    EncodeSettings loadEncodeSettings() const;
    void saveEncodeSettings(const EncodeSettings& s);

    QString ffmpegDir() const;
    void setFfmpegDir(const QString& dir);

    QString lastDir(const QString& key) const;
    void setLastDir(const QString& key, const QString& dir);

    QByteArray windowGeometry() const;
    void setWindowGeometry(const QByteArray& g);

private:
    mutable QSettings m_settings;
};

}  // namespace vidcut
