#include "BinaryLineImport_Widget.hpp"
#include "ui_BinaryLineImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QDir>

BinaryLineImport_Widget::BinaryLineImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BinaryLineImport_Widget) {
    ui->setupUi(this);
    connect(ui->load_binary_file_button, &QPushButton::clicked,
            this, &BinaryLineImport_Widget::_onLoadBinaryFileButtonPressed);
}

BinaryLineImport_Widget::~BinaryLineImport_Widget() {
    delete ui;
}

void BinaryLineImport_Widget::_onLoadBinaryFileButtonPressed() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Binary Line File"),
        QDir::currentPath(),
        tr("Binary files (*.bin *.capnp);;All files (*.*)"));

    if (!filePath.isNull() && !filePath.isEmpty()) {
        emit loadBinaryFileRequested(filePath);
    }
}
