#include "CSVPointSaver_Widget.hpp"
#include "ui_CSVPointSaver_Widget.h"

CSVPointSaver_Widget::CSVPointSaver_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CSVPointSaver_Widget)
{
    ui->setupUi(this);
    connect(ui->save_action_button, &QPushButton::clicked, this, [this]() {
        emit saveCSVRequested(ui->save_filename_edit->text());
    });
}

CSVPointSaver_Widget::~CSVPointSaver_Widget()
{
    delete ui;
} 