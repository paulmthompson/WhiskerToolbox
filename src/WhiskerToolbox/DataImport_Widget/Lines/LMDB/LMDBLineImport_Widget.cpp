#include "LMDBLineImport_Widget.hpp"
#include "ui_LMDBLineImport_Widget.h"

LMDBLineImport_Widget::LMDBLineImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LMDBLineImport_Widget) {
    ui->setupUi(this);
}

LMDBLineImport_Widget::~LMDBLineImport_Widget() {
    delete ui;
}
