#pragma once
#include <QMainWindow>
#include <QString>

#include "engine/SettingsStore.h"

class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QThread;
class QTimeEdit;

namespace vidcut {

class DurationProbe;
class SettingsPanel;
class SmartCutter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void browseInput();
    void browseOutput();
    void startCut();
    void cancelCut();
    void chooseFfmpegLocation();
    void showAbout();
    void checkFfmpegOnStartup();

    void onProbeSucceeded(const QString& path, double durationSeconds);
    void onProbeFailed(const QString& path, const QString& message);

    void onStage(const QString& label, int index, int count);
    void onProgress(double fraction, double speed, double etaSeconds);
    void onSucceeded(const QString& summary);
    void onFailed(const QString& message);
    void onCancelled();
    void onThreadFinished();

private:
    void buildUi();
    void buildMenus();
    void loadInput(const QString& path);  // fills the fields and reads the duration
    void clearInput();
    void updateControls();
    QString defaultOutputFor(const QString& input) const;

    SettingsStore m_store;

    QLineEdit* m_inputEdit = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QTimeEdit* m_startEdit = nullptr;
    QTimeEdit* m_endEdit = nullptr;
    QPushButton* m_inputBrowse = nullptr;
    QPushButton* m_outputBrowse = nullptr;
    SettingsPanel* m_settingsPanel = nullptr;
    QPushButton* m_cutButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_stageLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_speedLabel = nullptr;

    DurationProbe* m_probe = nullptr;
    QThread* m_thread = nullptr;
    SmartCutter* m_worker = nullptr;

    QString m_loadedPath;           // file whose duration we are reading / have read
    double m_inputDuration = 0.0;   // seconds; 0 = nothing loaded yet
    bool m_probing = false;
    bool m_cancelRequested = false;
    bool m_outputAutoFilled = true;  // output field was generated from the input name
};

}  // namespace vidcut
