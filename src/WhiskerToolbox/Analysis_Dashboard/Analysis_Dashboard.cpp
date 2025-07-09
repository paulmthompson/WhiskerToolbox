
#include "Analysis_Dashboard.hpp"
#include "ui_Analysis_Dashboard.h"

Analysis_Dashboard::Analysis_Dashboard(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::Analysis_Dashboard) {
    ui->setupUi(this);
}

Analysis_Dashboard::~Analysis_Dashboard() {
    delete ui;
}

void Analysis_Dashboard::openWidget() {
    this->show();
}
