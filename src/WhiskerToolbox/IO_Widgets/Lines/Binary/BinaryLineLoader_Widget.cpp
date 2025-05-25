#include "BinaryLineLoader_Widget.hpp"
#include "ui_BinaryLineLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QDir>

BinaryLineLoader_Widget::BinaryLineLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BinaryLineLoader_Widget)
{
    ui->setupUi(this);
    connect(ui->load_binary_file_button, &QPushButton::clicked,
            this, &BinaryLineLoader_Widget::_onLoadBinaryFileButtonPressed);
}

BinaryLineLoader_Widget::~BinaryLineLoader_Widget()
{
    delete ui;
}

void BinaryLineLoader_Widget::_onLoadBinaryFileButtonPressed()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Binary Line File"),
        QDir::currentPath(),
        tr("Binary files (*.bin *.capnp);;All files (*.*)"));

    if (!filePath.isNull() && !filePath.isEmpty()) {
        emit loadBinaryFileRequested(filePath);
    }
} 