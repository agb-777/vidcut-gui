#include "ui/PlanTableWidget.h"

#include <QHeaderView>
#include <QStringList>

namespace vidcut {

PlanTableWidget::PlanTableWidget(QWidget* parent) : QTableWidget(0, 4, parent) {
    setHorizontalHeaderLabels({QStringLiteral("Segment"), QStringLiteral("Mode"),
                               QStringLiteral("Range"), QStringLiteral("Duration")});
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    verticalHeader()->hide();
    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hide();
}

void PlanTableWidget::setPlan(const QVariantList& rows) {
    setRowCount(0);
    for (const QVariant& rv : rows) {
        const QStringList cols = rv.toStringList();
        if (cols.size() < 4) continue;
        const int r = rowCount();
        insertRow(r);
        for (int c = 0; c < 4; ++c) {
            auto* item = new QTableWidgetItem(cols.at(c));
            if (c == 0 || c == 3) item->setTextAlignment(Qt::AlignCenter);
            setItem(r, c, item);
        }
    }
    if (rowCount() == 0) {
        hide();
        return;
    }
    setFixedHeight(horizontalHeader()->height() + verticalHeader()->length() + 2 * frameWidth() + 2);
    show();
}

void PlanTableWidget::clearPlan() {
    setRowCount(0);
    hide();
}

}  // namespace vidcut
