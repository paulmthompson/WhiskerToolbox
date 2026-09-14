/**
 * @file LineSelectSelectionWidget.cpp
 * @brief Instructions panel for editing a selected line
 */

#include "LineSelectSelectionWidget.hpp"
#include "ui_LineSelectSelectionWidget.h"

namespace line_widget {

LineSelectSelectionWidget::LineSelectSelectionWidget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LineSelectSelectionWidget) {
    ui->setupUi(this);
}

LineSelectSelectionWidget::~LineSelectSelectionWidget() {
    delete ui;
}

}// namespace line_widget
