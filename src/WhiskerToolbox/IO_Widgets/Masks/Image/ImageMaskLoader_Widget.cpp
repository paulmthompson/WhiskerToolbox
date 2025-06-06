#include "ImageMaskLoader_Widget.hpp"
#include "ui_ImageMaskLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QMessageBox>

ImageMaskLoader_Widget::ImageMaskLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ImageMaskLoader_Widget)
{
    ui->setupUi(this);
    
    // Connect signals
    connect(ui->browse_directory_button, &QPushButton::clicked, 
            this, &ImageMaskLoader_Widget::_onBrowseDirectoryClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &ImageMaskLoader_Widget::_onLoadButtonClicked);
}

ImageMaskLoader_Widget::~ImageMaskLoader_Widget()
{
    delete ui;
}

void ImageMaskLoader_Widget::_onBrowseDirectoryClicked()
{
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

void ImageMaskLoader_Widget::_onLoadButtonClicked()
{
    QString directoryPath = ui->directory_path_edit->text().trimmed();
    
    if (directoryPath.isEmpty()) {
        QMessageBox::warning(this, "No Directory Selected", 
                           "Please select a directory containing mask images.");
        return;
    }
    
    // Collect options from UI
    ImageMaskLoaderOptions options;
    options.directory_path = directoryPath.toStdString();
    options.file_pattern = ui->file_pattern_combo->currentText().toStdString();
    options.filename_prefix = ui->filename_prefix_edit->text().toStdString();
    options.frame_number_padding = ui->frame_padding_spinbox->value();
    options.threshold_value = ui->threshold_spinbox->value();
    options.invert_mask = ui->invert_mask_checkbox->isChecked();
    
    // Emit signal with options
    emit loadImageMaskRequested(options);
} 