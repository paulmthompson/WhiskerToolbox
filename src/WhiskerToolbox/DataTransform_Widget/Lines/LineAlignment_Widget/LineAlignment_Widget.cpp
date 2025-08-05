#include "LineAlignment_Widget.hpp"

#include "ui_LineAlignment_Widget.h"

#include "DataManager.hpp"
#include "Media/Media_Data.hpp"

LineAlignment_Widget::LineAlignment_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::LineAlignment_Widget)
{
    ui->setupUi(this);

    // Setup the media data feature table widget
    ui->media_feature_table_widget->setColumns({"Feature", "Type"});
    
    // Setup the approach combo box
    ui->approachComboBox->addItem("Peak Width Half Max", static_cast<int>(FWHMApproach::PEAK_WIDTH_HALF_MAX));
    ui->approachComboBox->addItem("Gaussian Fit", static_cast<int>(FWHMApproach::GAUSSIAN_FIT));
    ui->approachComboBox->addItem("Threshold Based", static_cast<int>(FWHMApproach::THRESHOLD_BASED));
    
    // Set default values
    ui->widthSpinBox->setValue(20);
    ui->useProcessedDataCheckBox->setChecked(true);
    ui->approachComboBox->setCurrentIndex(0);
    
    // Connect signals and slots
    connect(ui->media_feature_table_widget, &Feature_Table_Widget::featureSelected, 
            this, &LineAlignment_Widget::_mediaFeatureSelected);
    connect(ui->widthSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LineAlignment_Widget::_widthValueChanged);
    connect(ui->useProcessedDataCheckBox, &QCheckBox::toggled,
            this, &LineAlignment_Widget::_useProcessedDataToggled);
    connect(ui->approachComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineAlignment_Widget::_approachChanged);
}

LineAlignment_Widget::~LineAlignment_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineAlignment_Widget::getParameters() const {
    auto params = std::make_unique<LineAlignmentParameters>();
    
    // Get selected media data key from the UI
    QString selectedFeature = ui->selectedMediaLineEdit->text();
    if (!selectedFeature.isEmpty() && _data_manager) {
        // Get the actual MediaData from the DataManager
        auto media_data_variant = _data_manager->getDataVariant(selectedFeature.toStdString());
        if (media_data_variant.has_value() && 
            std::holds_alternative<std::shared_ptr<MediaData>>(*media_data_variant)) {
            // Store the MediaData pointer directly in the parameters
            params->media_data = std::get<std::shared_ptr<MediaData>>(*media_data_variant);
        }
    }
    
    // Set other parameters from UI
    params->width = ui->widthSpinBox->value();
    params->use_processed_data = ui->useProcessedDataCheckBox->isChecked();
    params->approach = static_cast<FWHMApproach>(ui->approachComboBox->currentData().toInt());
    
    return params;
}

void LineAlignment_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    
    // Configure the feature table widget to only show media data
    ui->media_feature_table_widget->setDataManager(_data_manager);
    ui->media_feature_table_widget->setTypeFilter({DM_DataType::Images, DM_DataType::Video});
    ui->media_feature_table_widget->populateTable();
}

void LineAlignment_Widget::_mediaFeatureSelected(QString const & feature) {
    // Update the selected media data display
    ui->selectedMediaLineEdit->setText(feature);
}

void LineAlignment_Widget::_widthValueChanged(int value) {
    // Update the width label to show the current value
    ui->widthLabel->setText(QString("Width: %1 pixels").arg(value));
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
    // Update the approach description based on selection
    QString description;
    switch (static_cast<FWHMApproach>(ui->approachComboBox->currentData().toInt())) {
        case FWHMApproach::PEAK_WIDTH_HALF_MAX:
            description = "Find the width of the peak at half maximum intensity";
            break;
        case FWHMApproach::GAUSSIAN_FIT:
            description = "Fit a Gaussian curve to the intensity profile";
            break;
        case FWHMApproach::THRESHOLD_BASED:
            description = "Use threshold-based peak detection";
            break;
    }
    ui->approachDescriptionLabel->setText(description);
} 