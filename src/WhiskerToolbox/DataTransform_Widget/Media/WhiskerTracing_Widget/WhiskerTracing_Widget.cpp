#include "WhiskerTracing_Widget.hpp"

#include "ui_WhiskerTracing_Widget.h"

#include "DataManager/transforms/Media/whisker_tracing.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>

WhiskerTracing_Widget::WhiskerTracing_Widget(QWidget * parent)
    : TransformParameter_Widget(parent),
      ui(new Ui::WhiskerTracing_Widget) {
    ui->setupUi(this);

    // Connect signals to slots
    connect(ui->use_processed_data_checkbox, &QCheckBox::toggled, this, &WhiskerTracing_Widget::_onUseProcessedDataChanged);
    connect(ui->clip_length_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &WhiskerTracing_Widget::_onClipLengthChanged);
    connect(ui->whisker_length_threshold_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &WhiskerTracing_Widget::_onWhiskerLengthThresholdChanged);
    connect(ui->batch_size_spinbox, QOverload<int>::of(&QSpinBox::valueChanged), this, &WhiskerTracing_Widget::_onBatchSizeChanged);
    connect(ui->use_parallel_processing_checkbox, &QCheckBox::toggled, this, &WhiskerTracing_Widget::_onUseParallelProcessingChanged);
    connect(ui->use_mask_data_checkbox, &QCheckBox::toggled, this, &WhiskerTracing_Widget::_onUseMaskDataChanged);
    connect(ui->mask_data_combobox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WhiskerTracing_Widget::_onMaskDataChanged);

    // Set default values
    ui->use_processed_data_checkbox->setChecked(true);
    ui->clip_length_spinbox->setValue(0);
    ui->whisker_length_threshold_spinbox->setValue(50.0);
    ui->batch_size_spinbox->setValue(10);
    ui->use_parallel_processing_checkbox->setChecked(true);
    ui->use_mask_data_checkbox->setChecked(false);
    ui->mask_data_combobox->setEnabled(false);
}

WhiskerTracing_Widget::~WhiskerTracing_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> WhiskerTracing_Widget::getParameters() const {
    auto params = std::make_unique<WhiskerTracingParameters>();

    params->use_processed_data = ui->use_processed_data_checkbox->isChecked();
    params->clip_length = ui->clip_length_spinbox->value();
    params->whisker_length_threshold = static_cast<float>(ui->whisker_length_threshold_spinbox->value());
    params->batch_size = ui->batch_size_spinbox->value();
    params->use_parallel_processing = ui->use_parallel_processing_checkbox->isChecked();
    params->use_mask_data = ui->use_mask_data_checkbox->isChecked();

    // Get mask data from combobox selection
    if (params->use_mask_data && ui->mask_data_combobox->currentIndex() >= 0) {
        QString mask_key = ui->mask_data_combobox->currentText();
        // Note: The actual mask data will be resolved by the ParameterFactory
        // This is just a placeholder - the real resolution happens in the factory
    }

    return params;
}

void WhiskerTracing_Widget::_onUseProcessedDataChanged(bool checked) {
    static_cast<void>(checked);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onClipLengthChanged(int value) {
    static_cast<void>(value);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onWhiskerLengthThresholdChanged(double value) {
    static_cast<void>(value);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onBatchSizeChanged(int value) {
    static_cast<void>(value);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onUseParallelProcessingChanged(bool checked) {
    static_cast<void>(checked);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onUseMaskDataChanged(bool checked) {
    ui->mask_data_combobox->setEnabled(checked);
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onMaskDataChanged(int index) {
    static_cast<void>(index);
    // Parameters are updated when getParameters() is called
}
