#include "CSVDigitalIntervalLoader_Widget.hpp"
#include "ui_CSVDigitalIntervalLoader_Widget.h"

CSVDigitalIntervalLoader_Widget::CSVDigitalIntervalLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVDigitalIntervalLoader_Widget)
{
    ui->setupUi(this);
    connect(ui->load_csv_button, &QPushButton::clicked, this, [this]() {
        emit loadFileRequested(ui->delimiter_combo->currentText());
    });
}

CSVDigitalIntervalLoader_Widget::~CSVDigitalIntervalLoader_Widget()
{
    delete ui;
} 