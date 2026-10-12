#include "ui/SettingsPanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vidcut {

SettingsPanel::SettingsPanel(QWidget* parent) : QGroupBox(QStringLiteral("Encoding Settings"), parent) {
    m_preset = new QComboBox(this);
    for (const std::string& p : presetList()) m_preset->addItem(QString::fromStdString(p));
    m_preset->setToolTip(QStringLiteral("x264/x265 speed preset. Slower = smaller file / better quality."));

    m_crfSlider = new QSlider(Qt::Horizontal, this);
    m_crfSlider->setRange(kCrfMin, kCrfMax);
    m_crfSpin = new QSpinBox(this);
    m_crfSpin->setRange(kCrfMin, kCrfMax);
    const QString crfTip = QStringLiteral("Constant Rate Factor. Lower = higher quality and larger file.");
    m_crfSlider->setToolTip(crfTip);
    m_crfSpin->setToolTip(crfTip);
    connect(m_crfSlider, &QSlider::valueChanged, m_crfSpin, &QSpinBox::setValue);
    connect(m_crfSpin, &QSpinBox::valueChanged, m_crfSlider, &QSlider::setValue);

    m_bitrate = new QComboBox(this);
    m_bitrate->setEditable(true);
    m_bitrate->setInsertPolicy(QComboBox::NoInsert);
    for (const std::string& b : audioBitrateChoices()) m_bitrate->addItem(QString::fromStdString(b));
    m_bitrate->setToolTip(QStringLiteral("AAC audio bitrate. Pick one or type your own, e.g. 224k or 1M."));

    auto* crfRow = new QHBoxLayout;
    crfRow->addWidget(m_crfSlider, 1);
    crfRow->addWidget(m_crfSpin);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Preset:"), m_preset);
    form->addRow(QStringLiteral("CRF:"), crfRow);
    form->addRow(QStringLiteral("Audio bitrate:"), m_bitrate);

    auto* hint = new QLabel(
        QStringLiteral("Preset and CRF only affect the re-encoded parts (segments A and C, "
                       "or the whole range when smart cut is not possible). "
                       "Audio is always re-encoded to AAC."),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #666666;"));

    auto* reset = new QPushButton(QStringLiteral("Reset to Defaults"), this);
    connect(reset, &QPushButton::clicked, this, &SettingsPanel::resetToDefaults);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(hint, 1);
    bottom->addWidget(reset, 0, Qt::AlignTop);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addLayout(bottom);

    setSettings(defaultSettings());
}

EncodeSettings SettingsPanel::settings() const {
    EncodeSettings s;
    s.preset = m_preset->currentText().toStdString();
    s.crf = m_crfSpin->value();
    s.audioBitrate = m_bitrate->currentText().trimmed().toStdString();
    return s;
}

void SettingsPanel::setSettings(const EncodeSettings& s) {
    const int idx = m_preset->findText(QString::fromStdString(s.preset));
    m_preset->setCurrentIndex(idx >= 0 ? idx : 0);
    m_crfSpin->setValue(s.crf);
    m_bitrate->setCurrentText(QString::fromStdString(s.audioBitrate));
}

void SettingsPanel::resetToDefaults() { setSettings(defaultSettings()); }

}  // namespace vidcut
