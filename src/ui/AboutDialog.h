#pragma once
#include <QDialog>

namespace vidcut {

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

}  // namespace vidcut
