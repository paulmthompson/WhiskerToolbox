#include "LineAlignment_Widget.hpp"

#include "ui_LineAlignment_Widget.h"

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"

LineAlignment_Widget::LineAlignment_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::LineAlignment_Widget)
{
    ui->setupUi(this);

    // Setup approach combo box
    ui->approachComboBox->addItem("Peak Width Half Max", static_cast<int>(FWHMApproach::PEAK_WIDTH_HALF_MAX));
    ui->approachComboBox->addItem("Gaussian Fit", static_cast<int>(FWHMApproach::GAUSSIAN_FIT));
    ui->approachComboBox->addItem("Threshold Based", static_cast<int>(FWHMApproach::THRESHOLD_BASED));
    ui->approachComboBox->setCurrentIndex(0);

    // Setup output mode combo box
    ui->outputModeComboBox->addItem("Aligned Vertices", static_cast<int>(LineAlignmentOutputMode::ALIGNED_VERTICES));
    ui->outputModeComboBox->addItem("FWHM Profile Extents", static_cast<int>(LineAlignmentOutputMode::FWHM_PROFILE_EXTENTS));
    ui->outputModeComboBox->setCurrentIndex(0);

    // Set default values
    ui->widthSpinBox->setValue(20);
    ui->perpendicularRangeSpinBox->setValue(50);
    ui->useProcessedDataCheckBox->setChecked(true);
    ui->approachComboBox->setCurrentIndex(0);
    
    // Connect signals and slots
    connect(ui->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineAlignment_Widget::_widthValueChanged);
    connect(ui->perpendicularRangeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineAlignment_Widget::_perpendicularRangeValueChanged);
    connect(ui->useProcessedDataCheckBox, &QCheckBox::toggled,
            this, &LineAlignment_Widget::_useProcessedDataToggled);
    connect(ui->approachComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_approachChanged);
    connect(ui->outputModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_outputModeChanged);
}

LineAlignment_Widget::~LineAlignment_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineAlignment_Widget::getParameters() const {
    auto params = std::make_unique<LineAlignmentParameters>();
    
    // Auto-find the first available media data instead of requiring user selection
    if (_data_manager) {
        auto all_keys = _data_manager->getKeys<MediaData>();
        for (const auto& key : all_keys) {
            auto data_variant = _data_manager->getDataVariant(key);
            if (data_variant.has_value() &&
                std::holds_alternative<std::shared_ptr<MediaData>>(*data_variant)) {
                params->media_data = std::get<std::shared_ptr<MediaData>>(*data_variant);
                break; // Use the first media data found
            }
        }
    }
    
    // Set other parameters from UI
    params->width = ui->widthSpinBox->value();
    params->perpendicular_range = ui->perpendicularRangeSpinBox->value();
    params->use_processed_data = ui->useProcessedDataCheckBox->isChecked();
    params->approach = static_cast<FWHMApproach>(ui->approachComboBox->currentData().toInt());
    params->output_mode = static_cast<LineAlignmentOutputMode>(ui->outputModeComboBox->currentData().toInt());
    
    return params;
}

void LineAlignment_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
}

void LineAlignment_Widget::_widthValueChanged(int value) {
    // Update the width label to show the current value
    ui->widthLabel->setText(QString("Width: %1 pixels").arg(value));
}

void LineAlignment_Widget::_perpendicularRangeValueChanged(int value) {
    // Update the perpendicular range label to show the current value
    ui->perpendicularRangeLabel->setText(QString("Perpendicular Range: %1 pixels").arg(value));
}

void LineAlignment_Widget::_useProcessedDataToggled(bool checked) {
    // Update the description based on the selection
    if (checked) {
        ui->dataTypeLabel->setText("Using processed data (filtered/enhanced images)");
    } else {
        ui->dataTypeLabel->setText("Using raw data (original images)");
    }
}

void LineAlignment_Widget::_approachChanged(int index) {
    // Update the approach parameter
    if (index >= 0) {
        FWHMApproach approach = static_cast<FWHMApproach>(ui->approachComboBox->itemData(index).toInt());
        // You can add any additional logic here if needed
    }
}

void LineAlignment_Widget::_outputModeChanged(int index) {
    // Update the output mode parameter
    if (index >= 0) {
        LineAlignmentOutputMode output_mode = static_cast<LineAlignmentOutputMode>(ui->outputModeComboBox->itemData(index).toInt());
        // You can add any additional logic here if needed
    }
}
