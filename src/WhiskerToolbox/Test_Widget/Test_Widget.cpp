
#include "Test_Widget.hpp"
#include "ui_Test_Widget.h"

Test_Widget::Test_Widget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::Test_Widget) {
    ui->setupUi(this);
}

Test_Widget::~Test_Widget() {
    delete ui;
}
