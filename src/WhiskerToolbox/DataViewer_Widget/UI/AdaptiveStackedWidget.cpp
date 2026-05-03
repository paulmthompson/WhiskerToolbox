/**
 * @file AdaptiveStackedWidget.cpp
 * @brief Implementation of `AdaptiveStackedWidget`.
 */

#include "AdaptiveStackedWidget.hpp"

#include <QScrollArea>
#include <QShowEvent>
#include <QWidget>

AdaptiveStackedWidget::AdaptiveStackedWidget(QWidget * parent)
    : QStackedWidget(parent) {
    connect(this, &QStackedWidget::currentChanged, this, [this](int) { _propagateGeometryUpdate(); });
}

QSize AdaptiveStackedWidget::sizeHint() const {
    if (QWidget * const w = currentWidget()) {
        return w->sizeHint();
    }
    return QStackedWidget::sizeHint();
}

QSize AdaptiveStackedWidget::minimumSizeHint() const {
    if (QWidget * const w = currentWidget()) {
        return w->minimumSizeHint();
    }
    return QStackedWidget::minimumSizeHint();
}

void AdaptiveStackedWidget::showEvent(QShowEvent * event) {
    QStackedWidget::showEvent(event);
    _propagateGeometryUpdate();
}

void AdaptiveStackedWidget::_propagateGeometryUpdate() {
    updateGeometry();
    for (QWidget * ancestor = parentWidget(); ancestor != nullptr; ancestor = ancestor->parentWidget()) {
        ancestor->updateGeometry();
        if (qobject_cast<QScrollArea *>(ancestor) != nullptr) {
            break;
        }
    }
}
