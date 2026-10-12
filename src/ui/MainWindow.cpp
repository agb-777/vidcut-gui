#include "ui/MainWindow.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTime>
#include <QTimeEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <cmath>

#include "core/AppInfo.h"
#include "core/TimeUtils.h"
#include "engine/DurationProbe.h"
#include "engine/FfmpegLocator.h"
#include "engine/SmartCutter.h"
#include "ui/AboutDialog.h"
#include "ui/SettingsPanel.h"

namespace vidcut {

namespace {

const QStringList kOutputExts = {QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov")};

QString formatEta(double seconds) {
    const int s = static_cast<int>(std::lround(seconds));
    const int h = s / 3600, m = (s / 60) % 60, sec = s % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'));
}

int toSeconds(const QTime& t) { return t.hour() * 3600 + t.minute() * 60 + t.second(); }

QTime fromSeconds(int s) {
    s = qBound(0, s, 24 * 3600 - 1);  // the HH:MM:SS editor covers 00:00:00 - 23:59:59
    return QTime(s / 3600, (s / 60) % 60, s % 60);
}

// First regular file in a drag & drop payload (folders and web links are ignored).
QString droppedFilePath(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return QString();
    const QList<QUrl> urls = mime->urls();
    for (const QUrl& url : urls) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isFile()) return path;
    }
    return QString();
}

QTimeEdit* makeTimeEdit(QWidget* parent) {
    auto* e = new QTimeEdit(QTime(0, 0, 0), parent);
    e->setDisplayFormat(QStringLiteral("HH:mm:ss"));  // typing 2 digits jumps to the next block
    e->setButtonSymbols(QAbstractSpinBox::NoButtons);
    e->setAlignment(Qt::AlignCenter);
    e->setMinimumWidth(90);
    return e;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("%1 v%2")
                       .arg(QString::fromUtf8(appinfo::name), QString::fromUtf8(appinfo::version)));
    buildUi();
    buildMenus();

    m_probe = new DurationProbe(this);
    connect(m_probe, &DurationProbe::succeeded, this, &MainWindow::onProbeSucceeded);
    connect(m_probe, &DurationProbe::failed, this, &MainWindow::onProbeFailed);

    m_settingsPanel->setSettings(m_store.loadEncodeSettings());
    if (!m_store.windowGeometry().isEmpty()) restoreGeometry(m_store.windowGeometry());
    else resize(620, 430);

    // Drag & drop: accepted by the window; text fields must not swallow the drop.
    setAcceptDrops(true);
    const QList<QLineEdit*> edits = findChildren<QLineEdit*>();
    for (QLineEdit* e : edits) e->setAcceptDrops(false);
    const QList<QAbstractSpinBox*> spins = findChildren<QAbstractSpinBox*>();
    for (QAbstractSpinBox* s : spins) s->setAcceptDrops(false);

    updateControls();
    QTimer::singleShot(0, this, &MainWindow::checkFfmpegOnStartup);
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // --- files -------------------------------------------------------------
    auto* files = new QGroupBox(QStringLiteral("Files"), central);
    auto* fg = new QGridLayout(files);
    m_inputEdit = new QLineEdit(files);
    m_inputEdit->setReadOnly(true);  // chosen with Browse or drag & drop
    m_inputBrowse = new QPushButton(QStringLiteral("Browse..."), files);
    m_outputEdit = new QLineEdit(files);
    m_outputBrowse = new QPushButton(QStringLiteral("Browse..."), files);
    fg->addWidget(new QLabel(QStringLiteral("Input:"), files), 0, 0);
    fg->addWidget(m_inputEdit, 0, 1);
    fg->addWidget(m_inputBrowse, 0, 2);
    fg->addWidget(new QLabel(QStringLiteral("Output:"), files), 1, 0);
    fg->addWidget(m_outputEdit, 1, 1);
    fg->addWidget(m_outputBrowse, 1, 2);
    root->addWidget(files);

    connect(m_inputBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);
    connect(m_outputBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);
    connect(m_outputEdit, &QLineEdit::textEdited, this, [this]() { m_outputAutoFilled = false; });

    // --- time range -----------------------------------------------------------
    auto* range = new QGroupBox(QStringLiteral("Time Range"), central);
    auto* rg = new QHBoxLayout(range);
    m_startEdit = makeTimeEdit(range);
    m_endEdit = makeTimeEdit(range);
    rg->addWidget(new QLabel(QStringLiteral("Start:"), range));
    rg->addWidget(m_startEdit);
    rg->addSpacing(16);
    rg->addWidget(new QLabel(QStringLiteral("End:"), range));
    rg->addWidget(m_endEdit);
    rg->addStretch(1);
    root->addWidget(range);

    // --- settings ---------------------------------------------------------------
    m_settingsPanel = new SettingsPanel(central);
    root->addWidget(m_settingsPanel);

    // --- actions + progress ---------------------------------------------------
    auto* buttons = new QHBoxLayout;
    m_cutButton = new QPushButton(QStringLiteral("Cut"), central);
    m_cutButton->setDefault(true);
    m_cutButton->setMinimumWidth(110);
    m_cancelButton = new QPushButton(QStringLiteral("Cancel"), central);
    m_cancelButton->setMinimumWidth(110);
    buttons->addWidget(m_cutButton);
    buttons->addWidget(m_cancelButton);
    buttons->addStretch(1);
    root->addLayout(buttons);
    connect(m_cutButton, &QPushButton::clicked, this, &MainWindow::startCut);
    connect(m_cancelButton, &QPushButton::clicked, this, &MainWindow::cancelCut);

    m_stageLabel = new QLabel(QStringLiteral("Ready"), central);
    m_progress = new QProgressBar(central);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_speedLabel = new QLabel(central);
    root->addWidget(m_stageLabel);
    root->addWidget(m_progress);
    root->addWidget(m_speedLabel);
    root->addStretch(1);

    setCentralWidget(central);
}

void MainWindow::buildMenus() {
    QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* setFf = file->addAction(QStringLiteral("Set &FFmpeg Location..."));
    connect(setFf, &QAction::triggered, this, &MainWindow::chooseFfmpegLocation);
    file->addSeparator();
    QAction* quit = file->addAction(QStringLiteral("E&xit"));
    connect(quit, &QAction::triggered, this, &QWidget::close);

    QMenu* help = menuBar()->addMenu(QStringLiteral("&Help"));
    QAction* about = help->addAction(QStringLiteral("&About %1").arg(QString::fromUtf8(appinfo::name)));
    about->setMenuRole(QAction::AboutRole);
    connect(about, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::updateControls() {
    const bool running = (m_thread != nullptr);
    m_inputBrowse->setEnabled(!running);
    m_outputEdit->setEnabled(!running);
    m_outputBrowse->setEnabled(!running);
    m_startEdit->setEnabled(!running);
    m_endEdit->setEnabled(!running);
    m_settingsPanel->setEnabled(!running);
    m_cutButton->setEnabled(!running && !m_probing && m_inputDuration > 0.0);
    m_cancelButton->setEnabled(running && !m_cancelRequested);
}

QString MainWindow::defaultOutputFor(const QString& input) const {
    const QFileInfo fi(input);
    QString ext = fi.suffix().toLower();
    if (!kOutputExts.contains(ext)) ext = QStringLiteral("mp4");
    return QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QStringLiteral("_cut.") + ext);
}

// ---------------------------------------------------------------------------------
// loading an input file (Browse or drag & drop)
// ---------------------------------------------------------------------------------

void MainWindow::browseInput() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Input Video"), m_store.lastDir(QStringLiteral("input")),
        QStringLiteral("Video files (*.mp4 *.mkv *.mov *.avi *.webm *.ts *.m4v *.flv *.wmv *.mts *.m2ts);;All files (*)"));
    if (!path.isEmpty()) loadInput(path);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (!m_thread && !droppedFilePath(event->mimeData()).isEmpty()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    if (m_thread) return;
    const QString path = droppedFilePath(event->mimeData());
    if (path.isEmpty()) return;
    event->acceptProposedAction();
    loadInput(path);
}

void MainWindow::loadInput(const QString& path) {
    const FfmpegTools tools = FfmpegLocator::locate(m_store.ffmpegDir());
    if (!tools.isValid()) {
        QMessageBox::warning(this, QStringLiteral("FFmpeg not found"),
                             QStringLiteral("ffmpeg / ffprobe not found.\nUse File > Set FFmpeg Location... to choose them."));
        return;
    }

    const QString abs = QFileInfo(path).absoluteFilePath();
    m_loadedPath = abs;
    m_inputDuration = 0.0;
    m_inputEdit->setText(QDir::toNativeSeparators(abs));
    m_store.setLastDir(QStringLiteral("input"), QFileInfo(abs).absolutePath());
    if (m_outputAutoFilled || m_outputEdit->text().trimmed().isEmpty()) {
        m_outputEdit->setText(QDir::toNativeSeparators(defaultOutputFor(abs)));
        m_outputAutoFilled = true;
    }
    m_startEdit->setTime(QTime(0, 0, 0));
    m_endEdit->setTime(QTime(0, 0, 0));
    m_progress->setValue(0);
    m_speedLabel->clear();
    m_stageLabel->setText(QStringLiteral("Reading video info..."));

    m_probing = true;
    updateControls();
    m_probe->start(tools.ffprobe, abs);
}

void MainWindow::clearInput() {
    m_loadedPath.clear();
    m_inputDuration = 0.0;
    m_inputEdit->clear();
    if (m_outputAutoFilled) m_outputEdit->clear();
    m_startEdit->setTime(QTime(0, 0, 0));
    m_endEdit->setTime(QTime(0, 0, 0));
}

void MainWindow::onProbeSucceeded(const QString& path, double durationSeconds) {
    if (path != m_loadedPath) return;  // result of an older file
    m_probing = false;
    m_inputDuration = durationSeconds;
    // Round up to whole seconds: the cutter clamps the end to the real end of the video.
    m_endEdit->setTime(fromSeconds(static_cast<int>(std::ceil(durationSeconds - 0.001))));
    m_stageLabel->setText(QStringLiteral("Ready"));
    updateControls();
}

void MainWindow::onProbeFailed(const QString& path, const QString& message) {
    if (path != m_loadedPath) return;
    m_probing = false;
    clearInput();
    m_stageLabel->setText(QStringLiteral("Ready"));
    updateControls();
    QMessageBox::warning(this, QStringLiteral("VidCut"), message);
}

// ---------------------------------------------------------------------------------
// output / ffmpeg location / about
// ---------------------------------------------------------------------------------

void MainWindow::browseOutput() {
    QString start = m_outputEdit->text().trimmed();
    if (start.isEmpty()) start = m_store.lastDir(QStringLiteral("output"));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Select Output File"), start,
        QStringLiteral("Video files (*.mp4 *.mkv *.mov)"), nullptr,
        QFileDialog::DontConfirmOverwrite);  // we ask ourselves when starting
    if (path.isEmpty()) return;
    m_outputEdit->setText(QDir::toNativeSeparators(path));
    m_outputAutoFilled = false;
    m_store.setLastDir(QStringLiteral("output"), QFileInfo(path).absolutePath());
}

void MainWindow::checkFfmpegOnStartup() {
    if (FfmpegLocator::locate(m_store.ffmpegDir()).isValid()) return;
    QMessageBox::warning(
        this, QStringLiteral("FFmpeg not found"),
        QStringLiteral("VidCut needs ffmpeg and ffprobe, but could not find them.\n\n"
                       "Install FFmpeg, or use File > Set FFmpeg Location... to choose the folder "
                       "that contains both ffmpeg and ffprobe."));
}

void MainWindow::chooseFfmpegLocation() {
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select the folder containing ffmpeg and ffprobe"), m_store.ffmpegDir());
    if (dir.isEmpty()) return;
    if (!FfmpegLocator::findInDir(dir).isValid()) {
        QMessageBox::warning(this, QStringLiteral("FFmpeg not found"),
                             QStringLiteral("That folder does not contain both ffmpeg and ffprobe."));
        return;
    }
    m_store.setFfmpegDir(dir);
}

void MainWindow::showAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------------
// cutting
// ---------------------------------------------------------------------------------

void MainWindow::startCut() {
    if (m_thread || m_probing) return;

    auto warn = [this](const QString& msg) {
        QMessageBox::warning(this, QStringLiteral("VidCut"), msg);
    };

    const FfmpegTools tools = FfmpegLocator::locate(m_store.ffmpegDir());
    if (!tools.isValid()) {
        warn(QStringLiteral("ffmpeg / ffprobe not found.\nUse File > Set FFmpeg Location... to choose them."));
        return;
    }

    const QString input = m_loadedPath;
    if (input.isEmpty() || m_inputDuration <= 0.0 || !QFileInfo(input).isFile()) {
        warn(QStringLiteral("Please select an input video first."));
        return;
    }

    const int start = toSeconds(m_startEdit->time());
    const int end = toSeconds(m_endEdit->time());
    if (start >= end) {
        warn(QStringLiteral("Start time must be earlier than end time."));
        return;
    }
    if (start >= m_inputDuration) {
        warn(QStringLiteral("Start time is beyond the end of the video."));
        return;
    }

    const QString outText = m_outputEdit->text().trimmed();
    if (outText.isEmpty()) {
        warn(QStringLiteral("Please choose an output file."));
        return;
    }
    QString output = QFileInfo(outText).absoluteFilePath();
    if (QFileInfo(output).suffix().isEmpty()) {  // no extension typed: reuse the input's (or .mp4)
        QString ext = QFileInfo(input).suffix().toLower();
        if (!kOutputExts.contains(ext)) ext = QStringLiteral("mp4");
        output += QStringLiteral(".") + ext;
        m_outputEdit->setText(QDir::toNativeSeparators(output));
    }
    if (!kOutputExts.contains(QFileInfo(output).suffix().toLower())) {
        warn(QStringLiteral("Output extension must be .mp4, .mkv or .mov."));
        return;
    }
    if (!QFileInfo(QFileInfo(output).absolutePath()).isDir()) {
        warn(QStringLiteral("Output folder does not exist:\n%1")
                 .arg(QDir::toNativeSeparators(QFileInfo(output).absolutePath())));
        return;
    }
    if (QFileInfo::exists(output)) {
        const QString a = QFileInfo(input).canonicalFilePath();
        const QString b = QFileInfo(output).canonicalFilePath();
        if (!a.isEmpty() && a == b) {
            warn(QStringLiteral("Output must not be the same file as the input."));
            return;
        }
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Overwrite file?"),
            QStringLiteral("%1\nalready exists. Overwrite?").arg(QDir::toNativeSeparators(output)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
    }

    const EncodeSettings enc = m_settingsPanel->settings();
    const std::string problem = validateSettings(enc);
    if (!problem.empty()) {
        warn(QString::fromStdString(problem));
        return;
    }
    m_store.saveEncodeSettings(enc);

    CutJob job;
    job.input = input;
    job.output = output;
    job.start = start;
    job.end = end;
    job.settings = enc;
    job.ffmpeg = tools.ffmpeg;
    job.ffprobe = tools.ffprobe;

    m_progress->setValue(0);
    m_speedLabel->clear();
    m_stageLabel->setText(QStringLiteral("Starting..."));
    m_cancelRequested = false;

    m_thread = new QThread(this);
    m_worker = new SmartCutter(job);  // no parent: it is moved to the thread
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &SmartCutter::run);
    connect(m_worker, &SmartCutter::finishedRunning, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, &MainWindow::onThreadFinished);

    connect(m_worker, &SmartCutter::stageChanged, this, &MainWindow::onStage);
    connect(m_worker, &SmartCutter::progress, this, &MainWindow::onProgress);
    connect(m_worker, &SmartCutter::succeeded, this, &MainWindow::onSucceeded);
    connect(m_worker, &SmartCutter::failed, this, &MainWindow::onFailed);
    connect(m_worker, &SmartCutter::cancelled, this, &MainWindow::onCancelled);

    updateControls();
    m_thread->start();
}

void MainWindow::cancelCut() {
    if (!m_worker) return;
    m_cancelRequested = true;
    m_worker->requestCancel();
    m_stageLabel->setText(QStringLiteral("Cancelling..."));
    updateControls();
}

void MainWindow::onStage(const QString& label, int index, int count) {
    if (m_cancelRequested) return;
    m_stageLabel->setText(QStringLiteral("Step %1 of %2: %3").arg(index).arg(count).arg(label));
    m_progress->setValue(0);
    m_speedLabel->clear();
}

void MainWindow::onProgress(double fraction, double speed, double etaSeconds) {
    m_progress->setValue(static_cast<int>(std::lround(fraction * 1000.0)));
    QStringList parts;
    if (speed > 0) parts << QStringLiteral("Speed: %1x").arg(speed, 0, 'f', speed < 10 ? 2 : 0);
    if (etaSeconds >= 0) parts << QStringLiteral("ETA: %1").arg(formatEta(etaSeconds));
    m_speedLabel->setText(parts.join(QStringLiteral("    ")));
}

void MainWindow::onSucceeded(const QString& summary) {
    m_progress->setValue(1000);
    m_stageLabel->setText(QStringLiteral("Done"));
    m_speedLabel->clear();
    QMessageBox::information(this, QStringLiteral("Done"), summary);
}

void MainWindow::onFailed(const QString& message) {
    m_stageLabel->setText(QStringLiteral("Failed"));
    m_speedLabel->clear();
    QMessageBox::critical(this, QStringLiteral("Error"), message);
}

void MainWindow::onCancelled() {
    m_progress->setValue(0);
    m_stageLabel->setText(QStringLiteral("Cancelled"));
    m_speedLabel->clear();
}

void MainWindow::onThreadFinished() {
    if (m_thread) m_thread->deleteLater();
    m_thread = nullptr;
    m_worker = nullptr;  // freed through deleteLater (connected to QThread::finished)
    m_cancelRequested = false;
    updateControls();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_thread && m_thread->isRunning()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Exit"), QStringLiteral("A cut is still running. Cancel it and exit?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        if (m_worker) m_worker->requestCancel();
        m_thread->wait();  // returns once ffmpeg was killed and the worker finished
    }
    m_store.setWindowGeometry(saveGeometry());
    event->accept();
}

}  // namespace vidcut
