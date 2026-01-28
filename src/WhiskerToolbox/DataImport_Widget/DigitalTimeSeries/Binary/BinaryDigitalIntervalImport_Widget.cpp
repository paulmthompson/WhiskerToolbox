#include "BinaryDigitalIntervalImport_Widget.hpp"
#include "ui_BinaryDigitalIntervalImport_Widget.h"

#include <QPushButton>
#include <QFileDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QMessageBox>

BinaryDigitalIntervalImport_Widget::BinaryDigitalIntervalImport_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::BinaryDigitalIntervalImport_Widget) {
    ui->setupUi(this);
    
    connect(ui->browse_button, &QPushButton::clicked, 
            this, &BinaryDigitalIntervalImport_Widget::_onBrowseButtonClicked);
    connect(ui->load_button, &QPushButton::clicked, 
            this, &BinaryDigitalIntervalImport_Widget::_onLoadButtonClicked);
    connect(ui->data_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &BinaryDigitalIntervalImport_Widget::_onDataTypeChanged);
    
    // Set default data type to 2 bytes (16 channels) - index 1
    ui->data_type_combo->setCurrentIndex(1);
    _updateChannelRange();
}

BinaryDigitalIntervalImport_Widget::~BinaryDigitalIntervalImport_Widget() {
    delete ui;
}

void BinaryDigitalIntervalImport_Widget::_onBrowseButtonClicked() {
    QString selectedPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Binary File"),
        QString(),
        tr("Binary Files (*.bin *.dat);;All Files (*)"));
    
    if (!selectedPath.isEmpty()) {
        ui->file_path_edit->setText(selectedPath);
    }
}

void BinaryDigitalIntervalImport_Widget::_onLoadButtonClicked() {
    QString filePath = ui->file_path_edit->text().trimmed();
    
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("No File Selected"), 
                            tr("Please select a binary file to load."));
        return;
    }
    
    BinaryIntervalLoaderOptions options;
    options.filepath = filePath.toStdString();
    options.header_size_bytes = static_cast<size_t>(ui->header_size_spinbox->value());
    
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    switch (dataTypeIndex) {
        case 0: options.data_type_bytes = 1; break;
        case 1: options.data_type_bytes = 2; break;
        case 2: options.data_type_bytes = 4; break;
        case 3: options.data_type_bytes = 8; break;
        default: options.data_type_bytes = 2; break;
    }
    
    options.channel = ui->channel_spinbox->value();
    
    int transitionIndex = ui->transition_type_combo->currentIndex();
    options.transition_type = (transitionIndex == 0) ? "rising" : "falling";
    
    int max_channels = 0;
    switch (options.data_type_bytes) {
        case 1: max_channels = 8; break;
        case 2: max_channels = 16; break;
        case 4: max_channels = 32; break;
        case 8: max_channels = 64; break;
    }
    
    if (options.channel >= max_channels) {
        QMessageBox::warning(this, tr("Invalid Channel"), 
            tr("Channel %1 is out of range for %2-byte data type (max: %3)")
                .arg(options.channel)
                .arg(options.data_type_bytes)
                .arg(max_channels - 1));
        return;
    }
    
    emit loadBinaryIntervalRequested(options);
}

void BinaryDigitalIntervalImport_Widget::_onDataTypeChanged(int index) {
    Q_UNUSED(index)
    _updateChannelRange();
}

void BinaryDigitalIntervalImport_Widget::_updateChannelRange() {
    int dataTypeIndex = ui->data_type_combo->currentIndex();
    int max_channels = 0;
    
    switch (dataTypeIndex) {
        case 0: max_channels = 8; break;
        case 1: max_channels = 16; break;
        case 2: max_channels = 32; break;
        case 3: max_channels = 64; break;
        default: max_channels = 16; break;
    }
    
    ui->channel_spinbox->setMaximum(max_channels - 1);
    
    if (ui->channel_spinbox->value() >= max_channels) {
        ui->channel_spinbox->setValue(0);
    }
    
    ui->label_info->setText(tr("Channel range: 0 to %1 (%2 channels available)")
                           .arg(max_channels - 1)
                           .arg(max_channels));
}
