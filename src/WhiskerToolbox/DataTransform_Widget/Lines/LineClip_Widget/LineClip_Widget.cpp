#include "LineClip_Widget.hpp"
#include "ui_LineClip_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Lines/line_clip.hpp"
#include "DataManager/Lines/Line_Data.hpp"

LineClip_Widget::LineClip_Widget(QWidget *parent) :
    TransformParameter_Widget(parent),
    ui(new Ui::LineClip_Widget),
    _data_manager(nullptr)
{
    ui->setupUi(this);
    
    // Setup the line feature table widget
    ui->line_feature_table_widget->setColumns({"Feature", "Type"});
    
    // Set default values
    ui->referenceFrameSpinBox->setValue(0);
    ui->clipSideComboBox->setCurrentIndex(0); // Default to KeepBase
    
    // Connect signals
    connect(ui->line_feature_table_widget, &Feature_Table_Widget::featureSelected, 
            this, &LineClip_Widget::_lineFeatureSelected);
    connect(ui->clipSideComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LineClip_Widget::onClipSideChanged);
}

LineClip_Widget::~LineClip_Widget()
{
    delete ui;
}

void LineClip_Widget::setDataManager(std::shared_ptr<DataManager> data_manager)
{
    _data_manager = data_manager;
    
    // Configure the feature table widget to only show line data
    ui->line_feature_table_widget->setDataManager(_data_manager);
    ui->line_feature_table_widget->setTypeFilter({DM_DataType::Line});
    ui->line_feature_table_widget->populateTable();
}

std::unique_ptr<TransformParametersBase> LineClip_Widget::getParameters() const
{
    auto params = std::make_unique<LineClipParameters>();
    
    // Get selected reference line data
    QString selectedFeature = ui->selectedLineLineEdit->text();
    if (!selectedFeature.isEmpty() && _data_manager) {
        auto line_data_variant = _data_manager->getDataVariant(selectedFeature.toStdString());
        if (line_data_variant.has_value() && 
            std::holds_alternative<std::shared_ptr<LineData>>(*line_data_variant)) {
            params->reference_line_data = std::get<std::shared_ptr<LineData>>(*line_data_variant);
        }
    }
    
    // Get reference frame
    params->reference_frame = ui->referenceFrameSpinBox->value();
    
    // Get clip side
    int clipSideIndex = ui->clipSideComboBox->currentIndex();
    if (clipSideIndex == 0) {
        params->clip_side = ClipSide::KeepBase;
    } else {
        params->clip_side = ClipSide::KeepDistal;
    }
    
    return params;
}

void LineClip_Widget::_lineFeatureSelected(QString const & feature)
{
    // Update the selected line data display
    ui->selectedLineLineEdit->setText(feature);
    
    // Optionally, we could update the maximum frame number based on the selected line data
    if (_data_manager && !feature.isEmpty()) {
        auto line_data_variant = _data_manager->getDataVariant(feature.toStdString());
        if (line_data_variant.has_value() && 
            std::holds_alternative<std::shared_ptr<LineData>>(*line_data_variant)) {
            auto line_data = std::get<std::shared_ptr<LineData>>(*line_data_variant);
            auto times_with_data = line_data->getTimesWithData();
            if (!times_with_data.empty()) {
                TimeFrameIndex max_frame = *std::max_element(times_with_data.begin(), times_with_data.end());
                ui->referenceFrameSpinBox->setMaximum(max_frame.getValue());
                
                // Update the description to show available frames
                QString description = QString("Available frames: %1 to %2. The reference frame specifies which time point from the reference line data to use for clipping.")
                    .arg((*std::min_element(times_with_data.begin(), times_with_data.end())).getValue())
                    .arg(max_frame.getValue());
                ui->frameDescriptionLabel->setText(description);
            }
        }
    }
}

void LineClip_Widget::onClipSideChanged(int index)
{
    // Update preview text based on selected clip side
    QString previewText;
    if (index == 0) { // KeepBase
        previewText = "• Base side: Keeps the portion of the line from its starting point up to the intersection\n"
                     "• This preserves the beginning of the line and discards the end\n"
                     "• If no intersection is found, the original line is preserved unchanged";
    } else { // KeepDistal
        previewText = "• Distal side: Keeps the portion of the line from the intersection to its ending point\n"
                     "• This preserves the end of the line and discards the beginning\n"
                     "• If no intersection is found, the original line is preserved unchanged";
    }
    ui->previewLabel->setText(previewText);
} 
