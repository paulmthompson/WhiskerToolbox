/**
 * @file PenToolOptions_Widget.cpp
 * @brief Instruction-only options bar page for the Media Viewer Pen tool
 */

#include "PenToolOptions_Widget.hpp"
#include "ui_PenToolOptions_Widget.h"

PenToolOptions_Widget::PenToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PenToolOptions_Widget) {
    ui->setupUi(this);
}

PenToolOptions_Widget::~PenToolOptions_Widget() {
    delete ui;
}
