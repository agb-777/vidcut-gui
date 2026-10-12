#include "engine/FileUtils.h"

#include <QFile>
#include <filesystem>
#include <system_error>

namespace vidcut {

static std::filesystem::path toPath(const QString& s) {
#ifdef _WIN32
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(std::string(QFile::encodeName(s).constData()));
#endif
}

bool replaceFile(const QString& src, const QString& dst, QString* error) {
    std::error_code ec;
    std::filesystem::rename(toPath(src), toPath(dst), ec);  // atomically replaces dst
    if (!ec) return true;

    // Fallback: copy over, then delete the source.
    std::filesystem::copy_file(toPath(src), toPath(dst),
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error)
            *error = QStringLiteral("Cannot write output file (is it open in another program?): %1")
                         .arg(QString::fromStdString(ec.message()));
        return false;
    }
    QFile::remove(src);
    return true;
}

}  // namespace vidcut
