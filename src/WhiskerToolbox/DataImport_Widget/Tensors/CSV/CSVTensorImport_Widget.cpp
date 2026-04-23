/**
 * @file CSVTensorImport_Widget.cpp
 * @brief Implementation of the CSV tensor import widget
 */

#include "CSVTensorImport_Widget.hpp"
#include "ui_CSVTensorImport_Widget.h"

#include "IO/formats/CSV/tensors/Tensor_Data_CSV.hpp"
#include "StateManagement/AppFileDialog.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>


CSVTensorImport_Widget::CSVTensorImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVTensorImport_Widget) {
    ui->setupUi(this);

    connect(ui->browse_button, &QPushButton::clicked,
            this, &CSVTensorImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked,
            this, &CSVTensorImport_Widget::_onLoadButtonClicked);
}

CSVTensorImport_Widget::~CSVTensorImport_Widget() {
    delete ui;
}

void CSVTensorImport_Widget::_onBrowseButtonClicked() {
    QString const selectedPath = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("import_tensor_csv"),
            tr("Select CSV File"),
            tr("CSV Files (*.csv);;Text Files (*.txt);;All Files (*)"));

    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void CSVTensorImport_Widget::_onLoadButtonClicked() {
    QString const filePath = ui->file_path_edit->text().trimmed();

    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"),
                             tr("Please select a CSV file to load."));
        return;
    }

    CSVTensorLoaderOptions options;
    options.filepath = filePath.toStdString();

    QString const delimiterText = ui->delimiter_combo->currentText();
    if (delimiterText == "Comma") {
        options.delimiter = ",";
    } else if (delimiterText == "Space") {
        options.delimiter = " ";
    } else if (delimiterText == "Tab") {
        options.delimiter = "\t";
    } else if (delimiterText == "Semicolon") {
        options.delimiter = ";";
    }

    options.has_header = ui->has_header_checkbox->isChecked();

    emit loadTensorCSVRequested(options);
}
