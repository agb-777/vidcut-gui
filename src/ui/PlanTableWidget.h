#pragma once
#include <QTableWidget>
#include <QVariantList>

namespace vidcut {

// Small read-only table showing which parts are copied and which are re-encoded.
class PlanTableWidget : public QTableWidget {
    Q_OBJECT
public:
    explicit PlanTableWidget(QWidget* parent = nullptr);

public slots:
    void setPlan(const QVariantList& rows);  // each row: QStringList{segment, mode, range, duration}
    void clearPlan();
};

}  // namespace vidcut
