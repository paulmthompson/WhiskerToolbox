#include "WhiskerTracing_Widget.hpp"

#include "ui_WhiskerTracing_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Media/whisker_tracing.hpp"
#include "Masks/Mask_Data.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QSpinBox>

WhiskerTracing_Widget::WhiskerTracing_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
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
    ui->batch_size_spinbox->setValue(100);
    ui->use_parallel_processing_checkbox->setChecked(true);
    ui->use_mask_data_checkbox->setChecked(false);
    ui->mask_data_combobox->setEnabled(false);
}

WhiskerTracing_Widget::~WhiskerTracing_Widget() {
    delete ui;
}

void WhiskerTracing_Widget::onDataManagerChanged() {
    _refreshMaskKeys();
}

void WhiskerTracing_Widget::onDataManagerDataChanged() {
    _refreshMaskKeys();
}

std::unique_ptr<TransformParametersBase> WhiskerTracing_Widget::getParameters() const {
    auto params = std::make_unique<WhiskerTracingParameters>();

    params->use_processed_data = ui->use_processed_data_checkbox->isChecked();
    params->clip_length = ui->clip_length_spinbox->value();
    params->whisker_length_threshold = static_cast<float>(ui->whisker_length_threshold_spinbox->value());
    params->batch_size = ui->batch_size_spinbox->value();
    params->use_parallel_processing = ui->use_parallel_processing_checkbox->isChecked();
    params->use_mask_data = ui->use_mask_data_checkbox->isChecked();

    // Resolve MaskData pointer based on the current selection captured by slot handlers
    auto dm = dataManager();
    if (params->use_mask_data && dm && !_selected_mask_key.empty()) {
        try {
            auto mask_ptr = dm->getData<MaskData>(_selected_mask_key);
            if (mask_ptr) {
                params->mask_data = mask_ptr;
            }
        } catch (...) {
            // leave mask_data empty on failure
        }
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
    if (checked) {
        _refreshMaskKeys();
        auto dm = dataManager();
        bool const has_masks = dm && !dm->getKeys<MaskData>().empty();
        // If no masks are present, disable transform button via parent DataTransform widget
        if (auto parent_widget = parentWidget()) {
            auto do_btn = parent_widget->findChild<QPushButton *>("do_transform_button");
            if (do_btn) do_btn->setEnabled(has_masks);
        }
    } else {
        if (auto parent_widget = parentWidget()) {
            auto do_btn = parent_widget->findChild<QPushButton *>("do_transform_button");
            if (do_btn) do_btn->setEnabled(true);
        }
    }
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_onMaskDataChanged(int index) {
    if (index >= 0) {
        _selected_mask_key = ui->mask_data_combobox->itemText(index).toStdString();
    }
    // Parameters are updated when getParameters() is called
}

void WhiskerTracing_Widget::_refreshMaskKeys() {
    auto dm = dataManager();
    if (!dm) {
        ui->mask_data_combobox->clear();
        ui->mask_data_combobox->setEnabled(false);
        return;
    }
    _updateMaskComboBox();
    bool const has_masks = !dm->getKeys<MaskData>().empty();
    ui->mask_data_combobox->setEnabled(ui->use_mask_data_checkbox->isChecked() && has_masks);
}

void WhiskerTracing_Widget::_updateMaskComboBox() {
    auto dm = dataManager();
    if (!dm) return;

    QString current_text = ui->mask_data_combobox->currentText();
    ui->mask_data_combobox->clear();

    auto mask_keys = dm->getKeys<MaskData>();
    for (auto const & key: mask_keys) {
        ui->mask_data_combobox->addItem(QString::fromStdString(key));
    }

    if (!current_text.isEmpty()) {
        int idx = ui->mask_data_combobox->findText(current_text);
        if (idx >= 0) ui->mask_data_combobox->setCurrentIndex(idx);
    } else if (!mask_keys.empty()) {
        ui->mask_data_combobox->setCurrentIndex(0);
        _selected_mask_key = mask_keys[0];
    }
}
