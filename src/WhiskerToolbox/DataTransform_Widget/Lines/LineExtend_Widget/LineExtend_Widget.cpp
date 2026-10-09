#include "LineExtend_Widget.hpp"
#include "ui_LineExtend_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Extend/line_extend.hpp"
#include "Lines/Line_Data.hpp"

#include <algorithm>

LineExtend_Widget::LineExtend_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(new Ui::LineExtend_Widget) {
    ui->setupUi(this);

    ui->line_feature_table_widget->setColumns({"Feature", "Type"});

    ui->referenceFrameSpinBox->setValue(0);
    ui->extendEndComboBox->setCurrentIndex(1);
    ui->tangentDistancePixelsSpinBox->setValue(20.0);
    ui->tangentDistancePixelsSpinBox->setMinimum(0.1);

    connect(ui->line_feature_table_widget, &Feature_Table_Widget::featureSelected, this, &LineExtend_Widget::_lineFeatureSelected);
    connect(ui->extendEndComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &LineExtend_Widget::onExtendEndChanged);
    onExtendEndChanged(ui->extendEndComboBox->currentIndex());
}

LineExtend_Widget::~LineExtend_Widget() {
    delete ui;
}

void LineExtend_Widget::onDataManagerChanged() {
    auto dm = dataManager();
    ui->line_feature_table_widget->setDataManager(dm);
    ui->line_feature_table_widget->setTypeFilter({DM_DataType::Line});
    ui->line_feature_table_widget->populateTable();
}

void LineExtend_Widget::onDataManagerDataChanged() {
    ui->line_feature_table_widget->populateTable();
}

std::unique_ptr<TransformParametersBase> LineExtend_Widget::getParameters() const {
    auto params = std::make_unique<LineExtendParameters>();

    QString const selectedFeature = ui->selectedLineLineEdit->text();
    auto dm = dataManager();
    if (!selectedFeature.isEmpty() && dm) {
        auto line_data_variant = dm->getDataVariant(selectedFeature.toStdString());
        if (line_data_variant.has_value() &&
            std::holds_alternative<std::shared_ptr<LineData>>(*line_data_variant)) {
            params->reference_line_data = std::get<std::shared_ptr<LineData>>(*line_data_variant);
        }
    }

    params->reference_frame = ui->referenceFrameSpinBox->value();
    params->extend_end = ui->extendEndComboBox->currentIndex() == 0 ? ExtendEndpoint::Base : ExtendEndpoint::Distal;
    params->tangent_distance_pixels = static_cast<float>(ui->tangentDistancePixelsSpinBox->value());

    return params;
}

void LineExtend_Widget::_lineFeatureSelected(QString const & feature) {
    ui->selectedLineLineEdit->setText(feature);

    auto dm = dataManager();
    if (dm && !feature.isEmpty()) {
        auto line_data_variant = dm->getDataVariant(feature.toStdString());
        if (line_data_variant.has_value() &&
            std::holds_alternative<std::shared_ptr<LineData>>(*line_data_variant)) {
            auto line_data = std::get<std::shared_ptr<LineData>>(*line_data_variant);
            auto times_with_data = line_data->getTimesWithData();
            if (!times_with_data.empty()) {
                TimeFrameIndex const max_frame = *std::max_element(times_with_data.begin(), times_with_data.end());
                ui->referenceFrameSpinBox->setMaximum(static_cast<int>(max_frame.getValue()));

                QString const description = QString(
                                                    "Available frames: %1 to %2. The reference frame specifies which time "
                                                    "point from the reference line data to use for extension.")
                                                    .arg((*std::min_element(times_with_data.begin(), times_with_data.end())).getValue())
                                                    .arg(max_frame.getValue());
                ui->frameDescriptionLabel->setText(description);
            }
        }
    }
}

void LineExtend_Widget::onExtendEndChanged(int index) {
    QString previewText;
    if (index == 0) {
        previewText =
                "• Base: Extends the line backward from its starting point along the local tangent\n"
                "• The line grows until it intersects the reference polyline\n"
                "• If no intersection is found along the extension ray, the line is unchanged";
    } else {
        previewText =
                "• Distal: Extends the line forward from its ending point along the local tangent\n"
                "• The line grows until it intersects the reference polyline\n"
                "• If no intersection is found along the extension ray, the line is unchanged";
    }
    ui->previewLabel->setText(previewText);
}
