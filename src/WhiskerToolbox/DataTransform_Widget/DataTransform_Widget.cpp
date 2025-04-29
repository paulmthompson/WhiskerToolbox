

#include "DataTransform_Widget.hpp"
#include "ui_DataTransform_Widget.h"

#include "DataManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"


DataTransform_Widget::DataTransform_Widget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataTransform_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);
}

DataTransform_Widget::~DataTransform_Widget() {
    delete ui;
}

void DataTransform_Widget::openWidget() {
    ui->feature_table_widget->populateTable();
    this->show();
}

void DataTransform_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;

    auto key = feature.toStdString();
    auto feature_type = _data_manager->getType(feature.toStdString());
}
