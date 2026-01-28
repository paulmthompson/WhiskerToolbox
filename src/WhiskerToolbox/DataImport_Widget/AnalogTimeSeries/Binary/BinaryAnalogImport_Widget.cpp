#include "BinaryAnalogImport_Widget.hpp"
#include "ui_BinaryAnalogImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QMessageBox>

BinaryAnalogImport_Widget::BinaryAnalogImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BinaryAnalogImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &BinaryAnalogImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &BinaryAnalogImport_Widget::_onLoadButtonClicked);
    connect(ui->data_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &BinaryAnalogImport_Widget::_onDataTypeChanged);
    
    _updateInfoLabel();
}

BinaryAnalogImport_Widget::~BinaryAnalogImport_Widget() {
    delete ui;
}

void BinaryAnalogImport_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Binary File"),
        QString(),
        tr("Binary Files (*.bin *.dat *.raw);;All Files (*)"));
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void BinaryAnalogImport_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"), 
                            tr("Please select a binary file to load."));
        return;
    }
    
    BinaryAnalogLoaderOptions options;
    options.filepath = filePath.toStdString();
    options.header_size = ui->header_size_spinbox->value();
    options.num_channels = ui->num_channels_spinbox->value();
    options.use_memory_mapped = ui->memory_mapped_checkbox->isChecked();
    
    // Map combo box index to data type string
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    switch (dataTypeIndex) {
        case 0: options.binary_data_type = "int8"; break;
        case 1: options.binary_data_type = "int16"; break;
        case 2: options.binary_data_type = "uint16"; break;
        case 3: options.binary_data_type = "float32"; break;
        case 4: options.binary_data_type = "float64"; break;
        default: options.binary_data_type = "int16"; break;
    }
    
    options.scale_factor = static_cast<float>(ui->scale_factor_spinbox->value());
    options.offset_value = static_cast<float>(ui->offset_spinbox->value());
    options.offset = static_cast<size_t>(ui->sample_offset_spinbox->value());
    options.stride = static_cast<size_t>(ui->stride_spinbox->value());
    
    // Optional: number of samples (0 means read all)
    int numSamples = ui->num_samples_spinbox->value();
    if (numSamples > 0) {
        options.num_samples = static_cast<size_t>(numSamples);
    }
    
    emit loadBinaryAnalogRequested(options);
}

void BinaryAnalogImport_Widget::_onDataTypeChanged(int index) {
    Q_UNUSED(index)
    _updateInfoLabel();
}

void BinaryAnalogImport_Widget::_updateInfoLabel() {
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    int bytesPerSample = 0;
    QString typeDescription;
    
    switch (dataTypeIndex) {
        case 0: 
            bytesPerSample = 1; 
            typeDescription = "8-bit signed integer (-128 to 127)";
            break;
        case 1: 
            bytesPerSample = 2; 
            typeDescription = "16-bit signed integer (-32768 to 32767)";
            break;
        case 2: 
            bytesPerSample = 2; 
            typeDescription = "16-bit unsigned integer (0 to 65535)";
            break;
        case 3: 
            bytesPerSample = 4; 
            typeDescription = "32-bit floating point";
            break;
        case 4: 
            bytesPerSample = 8; 
            typeDescription = "64-bit floating point";
            break;
        default: 
            bytesPerSample = 2; 
            typeDescription = "Unknown";
            break;
    }
    
    ui->label_info->setText(tr("Data type: %1 (%2 bytes per sample)")
                           .arg(typeDescription)
                           .arg(bytesPerSample));
}
