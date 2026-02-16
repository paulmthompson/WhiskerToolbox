#include "BinaryLineImport_Widget.hpp"
#include "ui_BinaryLineImport_Widget.h"

#include <QPushButton>
#include <QDir>

#include "StateManagement/AppFileDialog.hpp"

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
    QString filePath = AppFileDialog::getOpenFileName(
        this,
        QStringLiteral("import_binary"),
        tr("Load Binary Line File"),
        tr("Binary files (*.bin *.capnp);;All files (*.*)"));

    if (!filePath.isNull() && !filePath.isEmpty()) {
        emit loadBinaryFileRequested(filePath);
    }
}
