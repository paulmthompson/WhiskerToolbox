#include "LineSelectSelectionWidget.hpp"
#include "ui_LineSelectSelectionWidget.h"

#include <QSpinBox>

namespace line_widget {

LineSelectSelectionWidget::LineSelectSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineSelectSelectionWidget) {
    ui->setupUi(this);
    
    // Connect the threshold spinbox to the signal
    connect(ui->threshold_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineSelectSelectionWidget::_onThresholdChanged);
}

LineSelectSelectionWidget::~LineSelectSelectionWidget() {
    delete ui;
}

float LineSelectSelectionWidget::getSelectionThreshold() const {
    return static_cast<float>(ui->threshold_spinbox->value());
}

void LineSelectSelectionWidget::setSelectionThreshold(float threshold) {
    ui->threshold_spinbox->blockSignals(true);
    ui->threshold_spinbox->setValue(static_cast<int>(threshold));
    ui->threshold_spinbox->blockSignals(false);
}

void LineSelectSelectionWidget::_onThresholdChanged(int threshold) {
    emit selectionThresholdChanged(static_cast<float>(threshold));
}

} // namespace line_widget 