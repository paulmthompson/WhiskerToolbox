#include "CSVPointLoader_Widget.hpp"
#include "ui_CSVPointLoader_Widget.h"

CSVPointLoader_Widget::CSVPointLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVPointLoader_Widget)
{
    ui->setupUi(this);
    connect(ui->load_single_csv_button, &QPushButton::clicked, this, [this]() {
        emit loadSingleCSVFileRequested(ui->delimiter_combo->currentText());
    });
}

CSVPointLoader_Widget::~CSVPointLoader_Widget()
{
    delete ui;
} 