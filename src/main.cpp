#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>

#include "core/AppInfo.h"
#include "ui/MainWindow.h"

// Plain white, "normal desktop application" look on every OS (ignores dark mode).
static void applyLightTheme(QApplication& app) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
#endif
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QPalette p;
    const QColor white(255, 255, 255), black(0, 0, 0), grey(240, 240, 240), accent(0, 120, 215);
    p.setColor(QPalette::Window, white);
    p.setColor(QPalette::WindowText, black);
    p.setColor(QPalette::Base, white);
    p.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
    p.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    p.setColor(QPalette::ToolTipText, black);
    p.setColor(QPalette::Text, black);
    p.setColor(QPalette::PlaceholderText, QColor(150, 150, 150));
    p.setColor(QPalette::Button, grey);
    p.setColor(QPalette::ButtonText, black);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, accent);
    p.setColor(QPalette::Highlight, accent);
    p.setColor(QPalette::HighlightedText, white);
    const QColor dis(160, 160, 160);
    p.setColor(QPalette::Disabled, QPalette::WindowText, dis);
    p.setColor(QPalette::Disabled, QPalette::Text, dis);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
    app.setPalette(p);
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("VidCut"));
    QCoreApplication::setApplicationName(QString::fromUtf8(vidcut::appinfo::name));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(vidcut::appinfo::version));

    applyLightTheme(app);

    vidcut::MainWindow window;
    window.show();
    return app.exec();
}
