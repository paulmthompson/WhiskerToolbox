#include "ImageMaskImport_Widget.hpp"
#include "ui_ImageMaskImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>

ImageMaskImport_Widget::ImageMaskImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ImageMaskImport_Widget) {
    ui->setupUi(this);
    
    // Connect signals
    connect(ui->browse_directory_button, &QPushButton::clicked, 
            this, &ImageMaskImport_Widget::_onBrowseDirectoryClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &ImageMaskImport_Widget::_onLoadButtonClicked);
}

ImageMaskImport_Widget::~ImageMaskImport_Widget() {
    delete ui;
}

void ImageMaskImport_Widget::_onBrowseDirectoryClicked() {
    QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        "Select Directory Containing Mask Images",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!selectedDirectory.isEmpty()) {
        ui->directory_path_edit->setText(selectedDirectory);
    }
}

void ImageMaskImport_Widget::_onLoadButtonClicked() {
    QString directoryPath = ui->directory_path_edit->text().trimmed();
    
    if (directoryPath.isEmpty()) {
        QMessageBox::warning(this, "No Directory Selected", 
                           "Please select a directory containing mask images.");
        return;
    }
    
    // Collect options from UI as JSON
    nlohmann::json config;
    config["directory_path"] = directoryPath.toStdString();
    config["file_pattern"] = ui->file_pattern_combo->currentText().toStdString();
    config["filename_prefix"] = ui->filename_prefix_edit->text().toStdString();
    config["frame_number_padding"] = ui->frame_padding_spinbox->value();
    config["threshold_value"] = ui->threshold_spinbox->value();
    config["invert_mask"] = ui->invert_mask_checkbox->isChecked();
    
    // Emit signal with JSON config
    emit loadImageMaskRequested("image", config);
}
