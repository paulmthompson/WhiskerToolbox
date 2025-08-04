#include "Test_Widget.hpp"
#include "ui_Test_Widget.h"
#include <QHeaderView>

Test_Widget::Test_Widget(QWidget* parent)
    : QWidget(parent),
      ui(new Ui::Test_Widget) {
    ui->setupUi(this);
    // Make columns resize to fill the available space
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
}

Test_Widget::~Test_Widget() {
    delete ui;
}
