#pragma once
#include <QGroupBox>

#include "core/EncodeSettings.h"

class QComboBox;
class QSlider;
class QSpinBox;

namespace vidcut {

// "Encoding Settings" box: preset, CRF and audio bitrate chosen by the user.
class SettingsPanel : public QGroupBox {
    Q_OBJECT
public:
    explicit SettingsPanel(QWidget* parent = nullptr);

    EncodeSettings settings() const;
    void setSettings(const EncodeSettings& s);

public slots:
    void resetToDefaults();

private:
    QComboBox* m_preset;
    QSpinBox* m_crfSpin;
    QSlider* m_crfSlider;
    QComboBox* m_bitrate;
};

}  // namespace vidcut
