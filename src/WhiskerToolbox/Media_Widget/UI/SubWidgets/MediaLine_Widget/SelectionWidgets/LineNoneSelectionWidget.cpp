#include "LineNoneSelectionWidget.hpp"
#include "ui_LineNoneSelectionWidget.h"

namespace line_widget {

LineNoneSelectionWidget::LineNoneSelectionWidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::LineNoneSelectionWidget) {
    ui->setupUi(this);
}

LineNoneSelectionWidget::~LineNoneSelectionWidget() {
    delete ui;
}

} // namespace line_widget 