#include "ui/AboutDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QtGlobal>

#include "core/AppInfo.h"

namespace vidcut {

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(QStringLiteral("About %1").arg(QString::fromUtf8(appinfo::name)));

    auto* title = new QLabel(QStringLiteral("<h2>%1</h2>").arg(QString::fromUtf8(appinfo::name)), this);
    auto* version = new QLabel(QStringLiteral("Version %1").arg(QString::fromUtf8(appinfo::version)), this);
    auto* qt = new QLabel(QStringLiteral("Built with Qt %1").arg(QString::fromUtf8(qVersion())), this);
    auto* desc = new QLabel(
        QStringLiteral("Fast, lossless-where-possible video cutter.\n"
                       "Uses FFmpeg: keyframe-aligned parts are copied without re-encoding, "
                       "only the cut edges are re-encoded."),
        this);
    desc->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addWidget(qt);
    layout->addSpacing(8);
    layout->addWidget(desc);
    layout->addSpacing(8);
    layout->addWidget(buttons);
    setMinimumWidth(380);
}

}  // namespace vidcut
