/**
 * @file CSVMaskSaver_Widget.cpp
 * @brief Implementation of CSVMaskSaver_Widget for saving MaskData as CSV RLE
 */
#include "CSVMaskSaver_Widget.hpp"
#include "ui_CSVMaskSaver_Widget.h"

#include "StateManagement/AppFileDialog.hpp"

#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

CSVMaskSaver_Widget::CSVMaskSaver_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::CSVMaskSaver_Widget) {
    ui->setupUi(this);

    connect(ui->browse_directory_button, &QPushButton::clicked,
            this, &CSVMaskSaver_Widget::_onBrowseDirectoryButtonClicked);
    connect(ui->save_button, &QPushButton::clicked,
            this, &CSVMaskSaver_Widget::_onSaveButtonClicked);
}

CSVMaskSaver_Widget::~CSVMaskSaver_Widget() {
    delete ui;
}

void CSVMaskSaver_Widget::_onBrowseDirectoryButtonClicked() {
    QString selectedDirectory = AppFileDialog::getExistingDirectory(
            this,
            QStringLiteral("export_mask_csv_dir"),
            "Select Output Directory for CSV Mask File");

    if (!selectedDirectory.isEmpty()) {
        ui->directory_path_edit->setText(selectedDirectory);
    }
}

void CSVMaskSaver_Widget::_onSaveButtonClicked() {
    QString const directoryPath = ui->directory_path_edit->text().trimmed();

    if (directoryPath.isEmpty()) {
        QMessageBox::warning(this, "No Directory Selected",
                             "Please select an output directory for the CSV file.");
        return;
    }

    QString const filename = ui->filename_edit->text().trimmed();
    if (filename.isEmpty()) {
        QMessageBox::warning(this, "No Filename",
                             "Please enter a filename for the CSV file.");
        return;
    }

    nlohmann::json config;
    config["parent_dir"] = directoryPath.toStdString();
    config["filename"] = filename.toStdString();

    emit saveCSVMaskRequested("csv", config);
}
