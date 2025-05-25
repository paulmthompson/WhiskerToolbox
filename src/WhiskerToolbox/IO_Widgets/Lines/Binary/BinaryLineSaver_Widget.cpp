#include "BinaryLineSaver_Widget.hpp"
#include "ui_BinaryLineSaver_Widget.h"

#include <QPushButton> // For QPushButton
#include <QLineEdit>   // For QLineEdit

BinaryLineSaver_Widget::BinaryLineSaver_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BinaryLineSaver_Widget)
{
    ui->setupUi(this);
    connect(ui->save_action_button, &QPushButton::clicked, this, [this]() {
        BinaryLineSaverOptions options;
        options.filename = ui->save_filename_edit->text().toStdString();
        // parent_dir will be set by the calling widget (Line_Widget)
        emit saveBinaryRequested(options);
    });
}

BinaryLineSaver_Widget::~BinaryLineSaver_Widget()
{
    delete ui;
} 