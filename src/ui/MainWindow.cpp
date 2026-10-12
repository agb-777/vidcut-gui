#include "ui/MainWindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
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
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>

#include "core/AppInfo.h"
#include "core/TimeUtils.h"
#include "engine/FfmpegLocator.h"
#include "engine/SmartCutter.h"
#include "ui/AboutDialog.h"
#include "ui/PlanTableWidget.h"
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
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("%1 v%2")
                       .arg(QString::fromUtf8(appinfo::name), QString::fromUtf8(appinfo::version)));
    buildUi();
    buildMenus();

    m_settingsPanel->setSettings(m_store.loadEncodeSettings());
    if (!m_store.windowGeometry().isEmpty()) restoreGeometry(m_store.windowGeometry());
    else resize(680, 760);

    setBusy(false);
    QTimer::singleShot(0, this, &MainWindow::checkFfmpegOnStartup);
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);

    // --- files -------------------------------------------------------------
    auto* files = new QGroupBox(QStringLiteral("Files"), central);
    auto* fg = new QGridLayout(files);
    m_inputEdit = new QLineEdit(files);
    m_inputEdit->setPlaceholderText(QStringLiteral("Select a video file..."));
    m_inputBrowse = new QPushButton(QStringLiteral("Browse..."), files);
    m_outputEdit = new QLineEdit(files);
    m_outputEdit->setPlaceholderText(QStringLiteral("Output file (.mp4, .mkv or .mov)"));
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
    auto* rg = new QGridLayout(range);
    m_startEdit = new QLineEdit(QStringLiteral("00:00:00"), range);
    m_endEdit = new QLineEdit(range);
    m_startEdit->setPlaceholderText(QStringLiteral("HH:MM:SS"));
    m_endEdit->setPlaceholderText(QStringLiteral("HH:MM:SS"));
    auto* rangeHint = new QLabel(QStringLiteral("Format: HH:MM:SS or HH:MM:SS.mmm  (e.g. 00:01:30 or 00:00:10.500)"), range);
    rangeHint->setStyleSheet(QStringLiteral("color: #666666;"));
    rg->addWidget(new QLabel(QStringLiteral("Start:"), range), 0, 0);
    rg->addWidget(m_startEdit, 0, 1);
    rg->addWidget(new QLabel(QStringLiteral("End:"), range), 0, 2);
    rg->addWidget(m_endEdit, 0, 3);
    rg->addWidget(rangeHint, 1, 0, 1, 4);
    rg->setColumnStretch(1, 1);
    rg->setColumnStretch(3, 1);
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
    m_speedLabel->setStyleSheet(QStringLiteral("color: #666666;"));
    root->addWidget(m_stageLabel);
    root->addWidget(m_progress);
    root->addWidget(m_speedLabel);

    // --- plan + log ---------------------------------------------------------------
    m_planTable = new PlanTableWidget(central);
    root->addWidget(m_planTable);

    m_log = new QPlainTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);
    m_log->setPlaceholderText(QStringLiteral("Log"));
    root->addWidget(m_log, 1);

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

void MainWindow::setBusy(bool busy) {
    for (QWidget* w : {static_cast<QWidget*>(m_inputEdit), static_cast<QWidget*>(m_outputEdit),
                       static_cast<QWidget*>(m_startEdit), static_cast<QWidget*>(m_endEdit),
                       static_cast<QWidget*>(m_inputBrowse), static_cast<QWidget*>(m_outputBrowse),
                       static_cast<QWidget*>(m_settingsPanel)})
        w->setEnabled(!busy);
    m_cutButton->setEnabled(!busy);
    m_cancelButton->setEnabled(busy);
}

QString MainWindow::defaultOutputFor(const QString& input) const {
    const QFileInfo fi(input);
    QString ext = fi.suffix().toLower();
    if (!kOutputExts.contains(ext)) ext = QStringLiteral("mp4");
    return QDir(fi.absolutePath()).filePath(fi.completeBaseName() + QStringLiteral("_cut.") + ext);
}

// ---------------------------------------------------------------------------------
// file selection
// ---------------------------------------------------------------------------------

void MainWindow::browseInput() {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Select Input Video"), m_store.lastDir(QStringLiteral("input")),
        QStringLiteral("Video files (*.mp4 *.mkv *.mov *.avi *.webm *.ts *.m4v *.flv *.wmv *.mts *.m2ts);;All files (*)"));
    if (path.isEmpty()) return;
    m_inputEdit->setText(QDir::toNativeSeparators(path));
    m_store.setLastDir(QStringLiteral("input"), QFileInfo(path).absolutePath());
    if (m_outputAutoFilled || m_outputEdit->text().trimmed().isEmpty()) {
        m_outputEdit->setText(QDir::toNativeSeparators(defaultOutputFor(path)));
        m_outputAutoFilled = true;
    }
}

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

// ---------------------------------------------------------------------------------
// FFmpeg location
// ---------------------------------------------------------------------------------

void MainWindow::checkFfmpegOnStartup() {
    const FfmpegTools t = FfmpegLocator::locate(m_store.ffmpegDir());
    if (t.isValid()) {
        appendLog(QStringLiteral("INFO  ffmpeg:  ") + QDir::toNativeSeparators(t.ffmpeg));
        appendLog(QStringLiteral("INFO  ffprobe: ") + QDir::toNativeSeparators(t.ffprobe));
        return;
    }
    appendLog(QStringLiteral("ERROR ffmpeg / ffprobe not found."));
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
    const FfmpegTools t = FfmpegLocator::findInDir(dir);
    if (!t.isValid()) {
        QMessageBox::warning(this, QStringLiteral("FFmpeg not found"),
                             QStringLiteral("That folder does not contain both ffmpeg and ffprobe."));
        return;
    }
    m_store.setFfmpegDir(dir);
    appendLog(QStringLiteral("INFO  ffmpeg location set: ") + QDir::toNativeSeparators(dir));
}

void MainWindow::showAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

// ---------------------------------------------------------------------------------
// cutting
// ---------------------------------------------------------------------------------

void MainWindow::startCut() {
    if (m_thread) return;  // previous run still shutting down

    auto warn = [this](const QString& msg) {
        QMessageBox::warning(this, QStringLiteral("VidCut"), msg);
    };

    const FfmpegTools tools = FfmpegLocator::locate(m_store.ffmpegDir());
    if (!tools.isValid()) {
        warn(QStringLiteral("ffmpeg / ffprobe not found.\nUse File > Set FFmpeg Location... to choose them."));
        return;
    }

    const QString input = QFileInfo(m_inputEdit->text().trimmed()).absoluteFilePath();
    if (m_inputEdit->text().trimmed().isEmpty() || !QFileInfo(input).isFile()) {
        warn(QStringLiteral("Please select an existing input file."));
        return;
    }

    const auto start = parseTime(m_startEdit->text().toStdString());
    if (!start) {
        warn(QStringLiteral("Invalid start time. Use HH:MM:SS (e.g. 00:01:30)."));
        return;
    }
    const auto end = parseTime(m_endEdit->text().toStdString());
    if (!end) {
        warn(QStringLiteral("Invalid end time. Use HH:MM:SS (e.g. 00:05:45)."));
        return;
    }
    if (*start >= *end) {
        warn(QStringLiteral("Start time must be earlier than end time."));
        return;
    }

    QString outText = m_outputEdit->text().trimmed();
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
    job.start = *start;
    job.end = *end;
    job.settings = enc;
    job.ffmpeg = tools.ffmpeg;
    job.ffprobe = tools.ffprobe;

    m_planTable->clearPlan();
    m_progress->setValue(0);
    m_speedLabel->clear();
    m_stageLabel->setText(QStringLiteral("Starting..."));
    appendLog(QStringLiteral("------------------------------------------------------------"));
    appendLog(QStringLiteral("INFO  %1  ->  %2  (%3 - %4)")
                  .arg(QDir::toNativeSeparators(input), QDir::toNativeSeparators(output),
                       QString::fromStdString(formatTime(*start)), QString::fromStdString(formatTime(*end))));

    m_thread = new QThread(this);
    m_worker = new SmartCutter(job);  // no parent: it is moved to the thread
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &SmartCutter::run);
    connect(m_worker, &SmartCutter::finishedRunning, m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, this, &MainWindow::onThreadFinished);

    connect(m_worker, &SmartCutter::log, this, &MainWindow::appendLog);
    connect(m_worker, &SmartCutter::planReady, m_planTable, &PlanTableWidget::setPlan);
    connect(m_worker, &SmartCutter::stageChanged, this, &MainWindow::onStage);
    connect(m_worker, &SmartCutter::progress, this, &MainWindow::onProgress);
    connect(m_worker, &SmartCutter::succeeded, this, &MainWindow::onSucceeded);
    connect(m_worker, &SmartCutter::failed, this, &MainWindow::onFailed);
    connect(m_worker, &SmartCutter::cancelled, this, &MainWindow::onCancelled);

    setBusy(true);
    m_thread->start();
}

void MainWindow::cancelCut() {
    if (!m_worker) return;
    m_worker->requestCancel();
    m_cancelButton->setEnabled(false);
    m_stageLabel->setText(QStringLiteral("Cancelling..."));
}

void MainWindow::appendLog(const QString& line) { m_log->appendPlainText(line); }

void MainWindow::onStage(const QString& label, int index, int count) {
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
    setBusy(false);
    m_progress->setValue(1000);
    m_stageLabel->setText(QStringLiteral("Done"));
    m_speedLabel->clear();
    appendLog(QStringLiteral("DONE"));
    appendLog(summary);
    QMessageBox::information(this, QStringLiteral("Done"), summary);
}

void MainWindow::onFailed(const QString& message) {
    setBusy(false);
    m_stageLabel->setText(QStringLiteral("Failed"));
    m_speedLabel->clear();
    appendLog(QStringLiteral("ERROR ") + message);
    QMessageBox::critical(this, QStringLiteral("Error"), message);
}

void MainWindow::onCancelled() {
    setBusy(false);
    m_progress->setValue(0);
    m_stageLabel->setText(QStringLiteral("Cancelled"));
    m_speedLabel->clear();
    appendLog(QStringLiteral("INFO  Cancelled by user."));
}

void MainWindow::onThreadFinished() {
    if (m_thread) m_thread->deleteLater();
    m_thread = nullptr;
    m_worker = nullptr;  // freed through deleteLater (connected to QThread::finished)
    setBusy(false);
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
