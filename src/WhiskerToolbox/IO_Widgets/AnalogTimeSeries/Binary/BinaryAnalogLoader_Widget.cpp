#include "BinaryAnalogLoader_Widget.hpp"
#include "ui_BinaryAnalogLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QMessageBox>

BinaryAnalogLoader_Widget::BinaryAnalogLoader_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BinaryAnalogLoader_Widget) {
    ui->setupUi(this);

    // Connect signals
    connect(ui->browse_button, &QPushButton::clicked, this, &BinaryAnalogLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, this, &BinaryAnalogLoader_Widget::_onLoadButtonClicked);
}

BinaryAnalogLoader_Widget::~BinaryAnalogLoader_Widget() {
    delete ui;
}

void BinaryAnalogLoader_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        "Select Binary File",
        "",
        "Binary Files (*.bin *.dat);;All Files (*)"
    );
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void BinaryAnalogLoader_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No File Selected", "Please select a binary file to load.");
        return;
    }
    
    BinaryAnalogLoaderOptions options;
    options.filename = filePath.toStdString();
    options.header_size = ui->header_size_spinbox->value();
    options.num_channels = ui->num_channels_spinbox->value();
    
    emit loadBinaryAnalogRequested(options);
} 