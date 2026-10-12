#pragma once
#include <QMainWindow>
#include <QVariantList>

#include "engine/SettingsStore.h"

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QThread;

namespace vidcut {

class PlanTableWidget;
class SettingsPanel;
class SmartCutter;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void browseInput();
    void browseOutput();
    void startCut();
    void cancelCut();
    void chooseFfmpegLocation();
    void showAbout();
    void checkFfmpegOnStartup();

    void appendLog(const QString& line);
    void onStage(const QString& label, int index, int count);
    void onProgress(double fraction, double speed, double etaSeconds);
    void onSucceeded(const QString& summary);
    void onFailed(const QString& message);
    void onCancelled();
    void onThreadFinished();

private:
    void buildUi();
    void buildMenus();
    void setBusy(bool busy);
    QString defaultOutputFor(const QString& input) const;

    SettingsStore m_store;

    QLineEdit* m_inputEdit = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QLineEdit* m_startEdit = nullptr;
    QLineEdit* m_endEdit = nullptr;
    QPushButton* m_inputBrowse = nullptr;
    QPushButton* m_outputBrowse = nullptr;
    SettingsPanel* m_settingsPanel = nullptr;
    QPushButton* m_cutButton = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_stageLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_speedLabel = nullptr;
    PlanTableWidget* m_planTable = nullptr;
    QPlainTextEdit* m_log = nullptr;

    QThread* m_thread = nullptr;
    SmartCutter* m_worker = nullptr;
    bool m_outputAutoFilled = true;  // output field was generated from the input name
};

}  // namespace vidcut
