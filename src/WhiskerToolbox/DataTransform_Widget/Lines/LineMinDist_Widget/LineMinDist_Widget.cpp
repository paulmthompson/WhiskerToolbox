#include "LineMinDist_Widget.hpp"

#include "ui_LineMinDist_Widget.h"

#include "DataManager.hpp"
#include "DataManager/transforms/Lines/Line_Min_Point_Dist/line_min_point_dist.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "Points/Point_Data.hpp"

LineMinDist_Widget::LineMinDist_Widget(QWidget *parent) :
      TransformParameter_Widget(parent),
      ui(new Ui::LineMinDist_Widget)
{
    ui->setupUi(this);

    // Setup the point data feature table widget
    ui->point_feature_table_widget->setColumns({"Feature", "Type"});
    
    // Connect signals and slots
    connect(ui->point_feature_table_widget, &Feature_Table_Widget::featureSelected, 
            this, &LineMinDist_Widget::_pointFeatureSelected);
}

LineMinDist_Widget::~LineMinDist_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> LineMinDist_Widget::getParameters() const {
    auto params = std::make_unique<LineMinPointDistParameters>();
    
    // Get selected point data key from the UI
    QString selectedFeature = ui->selectedPointLineEdit->text();
    if (!selectedFeature.isEmpty() && _data_manager) {
        // Get the actual PointData from the DataManager
        auto point_data_variant = _data_manager->getDataVariant(selectedFeature.toStdString());
        if (point_data_variant.has_value() && 
            std::holds_alternative<std::shared_ptr<PointData>>(*point_data_variant)) {
            // Store the PointData pointer directly in the parameters
            params->point_data = std::get<std::shared_ptr<PointData>>(*point_data_variant);
        }
    }
    
    return params;
}

void LineMinDist_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = data_manager;
    
    // Configure the feature table widget to only show point data
    ui->point_feature_table_widget->setDataManager(_data_manager);
    ui->point_feature_table_widget->setTypeFilter({DM_DataType::Points});
    ui->point_feature_table_widget->populateTable();
}

void LineMinDist_Widget::_pointFeatureSelected(QString const & feature) {
    // Update the selected point data display
    ui->selectedPointLineEdit->setText(feature);
}
