#include "ImageMaskSaver_Widget.hpp"
#include "ui_ImageMaskSaver_Widget.h"

#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QMessageBox>

ImageMaskSaver_Widget::ImageMaskSaver_Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ImageMaskSaver_Widget)
{
    ui->setupUi(this);

    // Connect signals
    connect(ui->browse_directory_button, &QPushButton::clicked, 
            this, &ImageMaskSaver_Widget::_onBrowseDirectoryButtonClicked);
    connect(ui->save_button, &QPushButton::clicked, 
            this, &ImageMaskSaver_Widget::_onSaveButtonClicked);
}

ImageMaskSaver_Widget::~ImageMaskSaver_Widget()
{
    delete ui;
}

void ImageMaskSaver_Widget::_onBrowseDirectoryButtonClicked()
{
    QString selectedDirectory = QFileDialog::getExistingDirectory(
        this,
        "Select Output Directory for Mask Images",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (!selectedDirectory.isEmpty()) {
        ui->directory_path_edit->setText(selectedDirectory);
    }
}

void ImageMaskSaver_Widget::_onSaveButtonClicked()
{
    QString directoryPath = ui->directory_path_edit->text().trimmed();
    
    if (directoryPath.isEmpty()) {
        QMessageBox::warning(this, "No Directory Selected", 
                           "Please select an output directory for the mask images.");
        return;
    }
    
    // Create options structure
    ImageMaskSaverOptions options;
    options.parent_dir = directoryPath.toStdString();
    options.image_format = ui->image_format_combo->currentText().toStdString();
    options.filename_prefix = ui->filename_prefix_edit->text().toStdString();
    options.frame_number_padding = ui->frame_padding_spinbox->value();
    options.image_width = ui->image_width_spinbox->value();
    options.image_height = ui->image_height_spinbox->value();
    options.background_value = ui->background_value_spinbox->value();
    options.mask_value = ui->mask_value_spinbox->value();
    options.overwrite_existing = ui->overwrite_existing_checkbox->isChecked();
    
    // Validate image dimensions
    if (options.image_width <= 0 || options.image_height <= 0) {
        QMessageBox::warning(this, "Invalid Dimensions", 
                           "Image width and height must be greater than 0.");
        return;
    }
    
    // Validate pixel values
    if (options.background_value == options.mask_value) {
        QMessageBox::warning(this, "Invalid Pixel Values", 
                           "Background value and mask value cannot be the same.");
        return;
    }
    
    emit saveImageMaskRequested(options);
} 
