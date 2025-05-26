#include "BinaryDigitalIntervalLoader_Widget.hpp"
#include "ui_BinaryDigitalIntervalLoader_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QMessageBox>

BinaryDigitalIntervalLoader_Widget::BinaryDigitalIntervalLoader_Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BinaryDigitalIntervalLoader_Widget)
{
    ui->setupUi(this);
    
    // Connect signals
    connect(ui->browse_button, &QPushButton::clicked, this, &BinaryDigitalIntervalLoader_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, this, &BinaryDigitalIntervalLoader_Widget::_onLoadButtonClicked);
    connect(ui->data_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &BinaryDigitalIntervalLoader_Widget::_onDataTypeChanged);
    
    // Set default data type to 2 bytes (16 channels) - index 1
    ui->data_type_combo->setCurrentIndex(1);
    _updateChannelRange();
}

BinaryDigitalIntervalLoader_Widget::~BinaryDigitalIntervalLoader_Widget()
{
    delete ui;
}

void BinaryDigitalIntervalLoader_Widget::_onBrowseButtonClicked() {
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

void BinaryDigitalIntervalLoader_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "No File Selected", "Please select a binary file to load.");
        return;
    }
    
    BinaryIntervalLoaderOptions options;
    options.filepath = filePath.toStdString();
    options.header_size_bytes = static_cast<size_t>(ui->header_size_spinbox->value());
    
    // Get data type bytes from combo box index
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    switch (dataTypeIndex) {
        case 0: options.data_type_bytes = 1; break;  // 1 byte (8 channels)
        case 1: options.data_type_bytes = 2; break;  // 2 bytes (16 channels)
        case 2: options.data_type_bytes = 4; break;  // 4 bytes (32 channels)
        case 3: options.data_type_bytes = 8; break;  // 8 bytes (64 channels)
        default: options.data_type_bytes = 2; break; // Default to 2 bytes
    }
    
    options.channel = ui->channel_spinbox->value();
    
    // Get transition type
    int transitionIndex = ui->transition_type_combo->currentIndex();
    options.transition_type = (transitionIndex == 0) ? "rising" : "falling";
    
    // Validate channel range
    int max_channels = 0;
    switch (options.data_type_bytes) {
        case 1: max_channels = 8; break;
        case 2: max_channels = 16; break;
        case 4: max_channels = 32; break;
        case 8: max_channels = 64; break;
    }
    
    if (options.channel >= max_channels) {
        QMessageBox::warning(this, "Invalid Channel", 
                            QString("Channel %1 is out of range for %2-byte data type (max: %3)")
                            .arg(options.channel)
                            .arg(options.data_type_bytes)
                            .arg(max_channels - 1));
        return;
    }
    
    emit loadBinaryIntervalRequested(options);
}

void BinaryDigitalIntervalLoader_Widget::_onDataTypeChanged(int index) {
    Q_UNUSED(index)
    _updateChannelRange();
}

void BinaryDigitalIntervalLoader_Widget::_updateChannelRange() {
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    int max_channels = 0;
    
    switch (dataTypeIndex) {
        case 0: max_channels = 8; break;   // 1 byte
        case 1: max_channels = 16; break;  // 2 bytes
        case 2: max_channels = 32; break;  // 4 bytes
        case 3: max_channels = 64; break;  // 8 bytes
        default: max_channels = 16; break; // Default
    }
    
    // Update channel spinbox maximum
    ui->channel_spinbox->setMaximum(max_channels - 1);
    
    // Reset channel to 0 if current value is out of range
    if (ui->channel_spinbox->value() >= max_channels) {
        ui->channel_spinbox->setValue(0);
    }
    
    // Update info label
    ui->label_info->setText(QString("Channel range: 0 to %1 (%2 channels available)")
                           .arg(max_channels - 1)
                           .arg(max_channels));
} 