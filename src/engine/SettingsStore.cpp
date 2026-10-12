#include "engine/SettingsStore.h"

namespace vidcut {

EncodeSettings SettingsStore::loadEncodeSettings() const {
    EncodeSettings s = defaultSettings();
    s.preset = m_settings.value(QStringLiteral("encode/preset"), QString::fromStdString(s.preset))
                   .toString().toStdString();
    s.crf = m_settings.value(QStringLiteral("encode/crf"), s.crf).toInt();
    s.audioBitrate =
        m_settings.value(QStringLiteral("encode/audioBitrate"), QString::fromStdString(s.audioBitrate))
            .toString().toStdString();
    if (!validateSettings(s).empty()) return defaultSettings();  // corrupted/edited by hand
    return s;
}

void SettingsStore::saveEncodeSettings(const EncodeSettings& s) {
    m_settings.setValue(QStringLiteral("encode/preset"), QString::fromStdString(s.preset));
    m_settings.setValue(QStringLiteral("encode/crf"), s.crf);
    m_settings.setValue(QStringLiteral("encode/audioBitrate"), QString::fromStdString(s.audioBitrate));
}

QString SettingsStore::ffmpegDir() const {
    return m_settings.value(QStringLiteral("paths/ffmpegDir")).toString();
}
void SettingsStore::setFfmpegDir(const QString& dir) {
    m_settings.setValue(QStringLiteral("paths/ffmpegDir"), dir);
}

QString SettingsStore::lastDir(const QString& key) const {
    return m_settings.value(QStringLiteral("paths/") + key).toString();
}
void SettingsStore::setLastDir(const QString& key, const QString& dir) {
    m_settings.setValue(QStringLiteral("paths/") + key, dir);
}

QByteArray SettingsStore::windowGeometry() const {
    return m_settings.value(QStringLiteral("ui/geometry")).toByteArray();
}
void SettingsStore::setWindowGeometry(const QByteArray& g) {
    m_settings.setValue(QStringLiteral("ui/geometry"), g);
}

}  // namespace vidcut
