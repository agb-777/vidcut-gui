#include "engine/MediaProbe.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <cmath>

#include "core/NumParse.h"
#include "engine/CutErrors.h"

namespace vidcut {

namespace {

inline QLatin1String K(const char* s) { return QLatin1String(s); }

std::string toStd(const QByteArray& b) { return std::string(b.constData(), static_cast<std::size_t>(b.size())); }

double jsonToDouble(const QJsonValue& v) {
    if (v.isDouble()) return v.toDouble();
    if (v.isString()) {
        double d = 0.0;
        if (parseDouble(v.toString().toStdString(), d)) return d;
    }
    return 0.0;
}

std::string str(const QJsonObject& o, const char* key) {
    return o.value(K(key)).toString().toStdString();
}

// Same rules as the original script: display-matrix side data first, then the old "rotate" tag.
int rotationOf(const QJsonObject& stream) {
    const QJsonArray sideData = stream.value(K("side_data_list")).toArray();
    for (const QJsonValue& sdv : sideData) {
        const QJsonObject sd = sdv.toObject();
        if (sd.contains(K("rotation"))) {
            const QJsonValue r = sd.value(K("rotation"));
            if (!r.isDouble() && !r.isString()) return 0;
            return static_cast<int>(std::lround(jsonToDouble(r)));
        }
    }
    const QJsonValue tag = stream.value(K("tags")).toObject().value(K("rotate"));
    if (!tag.isDouble() && !tag.isString()) return 0;
    return -static_cast<int>(std::lround(jsonToDouble(tag)));
}

}  // namespace

MediaInfo MediaProbe::probe(ProcessRunner& runner, const QString& ffprobe, const QString& path) {
    const QByteArray out = runner.capture(
        ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-print_format"),
                  QStringLiteral("json"), QStringLiteral("-show_format"),
                  QStringLiteral("-show_streams"), path});

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(out, &pe);
    if (doc.isNull() || !doc.isObject())
        throw CutError(QStringLiteral("Cannot parse ffprobe output: %1").arg(pe.errorString()));
    const QJsonObject root = doc.object();

    MediaInfo mi;
    bool foundVideo = false;
    for (const QJsonValue& sv : root.value(K("streams")).toArray()) {
        const QJsonObject s = sv.toObject();
        const QString type = s.value(K("codec_type")).toString();
        if (type == QLatin1String("audio")) mi.hasAudio = true;
        const bool attachedPic =
            s.value(K("disposition")).toObject().value(K("attached_pic")).toInt(0) != 0;
        if (type == QLatin1String("video") && !foundVideo && !attachedPic) {
            VideoInfo& v = mi.video;
            v.index = s.value(K("index")).toInt(0);
            v.codecName = str(s, "codec_name");
            v.profile = str(s, "profile");
            v.pixFmt = str(s, "pix_fmt");
            const QJsonValue lv = s.value(K("level"));
            v.level = lv.isDouble() ? static_cast<int>(lv.toDouble()) : 0;
            v.colorSpace = str(s, "color_space");
            v.colorPrimaries = str(s, "color_primaries");
            v.colorTransfer = str(s, "color_transfer");
            v.colorRange = str(s, "color_range");
            v.rotation = rotationOf(s);
            foundVideo = true;
        }
    }
    if (!foundVideo) throw CutError(QStringLiteral("No video stream found in this file."));

    const QJsonObject fmt = root.value(K("format")).toObject();
    mi.duration = jsonToDouble(fmt.value(K("duration")));
    mi.startTime = jsonToDouble(fmt.value(K("start_time")));
    return mi;
}

std::optional<std::vector<Packet>> MediaProbe::readPackets(ProcessRunner& runner,
                                                           const QString& ffprobe,
                                                           const QString& path, int streamIndex,
                                                           double startOffset) {
    const QByteArray out = runner.capture(
        ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
                  QString::number(streamIndex), QStringLiteral("-show_entries"),
                  QStringLiteral("packet=pts_time,flags"), QStringLiteral("-of"),
                  QStringLiteral("csv=p=0"), path});
    return parsePackets(toStd(out), startOffset);
}

double MediaProbe::readDuration(ProcessRunner& runner, const QString& ffprobe,
                                const QString& path) {
    const QByteArray out = runner.capture(
        ffprobe, {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-show_entries"),
                  QStringLiteral("format=duration"), QStringLiteral("-of"),
                  QStringLiteral("csv=p=0"), path});
    double d = 0.0;
    return parseDouble(toStd(out.trimmed()), d) ? d : 0.0;
}

}  // namespace vidcut
