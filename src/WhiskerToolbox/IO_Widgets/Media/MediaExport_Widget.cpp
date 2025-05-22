#include "MediaExport_Widget.hpp"
#include "ui_MediaExport_Widget.h"

#include <QCheckBox> // For connecting to save_by_frame_name_checkbox toggled signal
#include <QLineEdit> // For enabling/disabling image_name_prefix_edit
#include <QSpinBox>  // For enabling/disabling frame_id_padding_spinbox
#include <QLabel>    // For enabling/disabling labels

MediaExport_Widget::MediaExport_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MediaExport_Widget)
{
    ui->setupUi(this);

    connect(ui->save_by_frame_name_checkbox, &QCheckBox::toggled, this, &MediaExport_Widget::_updatePrefixAndPaddingState);

    // Set initial state of dependent widgets
    _updatePrefixAndPaddingState(ui->save_by_frame_name_checkbox->isChecked());
}

MediaExport_Widget::~MediaExport_Widget()
{
    delete ui;
}

MediaExportOptions MediaExport_Widget::getOptions() const
{
    MediaExportOptions options;
    options.save_by_frame_name = ui->save_by_frame_name_checkbox->isChecked();
    if (!options.save_by_frame_name) { // Only populate these if not saving by frame name
        options.frame_id_padding = ui->frame_id_padding_spinbox->value();
        options.image_name_prefix = ui->image_name_prefix_edit->text().toStdString();
    } else {
        // Explicitly set to default or clear if saving by frame name, 
        // though their UI elements are disabled, good for clarity.
        options.frame_id_padding = 7; // Default from MediaExportOptions struct or UI
        options.image_name_prefix = "img"; // Default from MediaExportOptions struct or UI
    }
    options.image_folder = ui->image_folder_edit->text().toStdString();
    // options.image_save_dir is intentionally not set here
    return options;
}

void MediaExport_Widget::_updatePrefixAndPaddingState(bool checked)
{
    // If "Save by Frame Name" is checked, disable prefix and padding fields
    ui->frame_id_padding_spinbox->setEnabled(!checked);
    ui->image_name_prefix_edit->setEnabled(!checked);
    // Also update the corresponding labels for better visual feedback
    ui->label_frame_id_padding->setEnabled(!checked);
    ui->label_image_name_prefix->setEnabled(!checked);
} 
