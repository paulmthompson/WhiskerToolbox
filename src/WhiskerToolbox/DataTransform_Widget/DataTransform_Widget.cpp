

#include "DataTransform_Widget.hpp"

#include "ui_DataTransform_Widget.h"

#include "DataManager.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "transforms/TransformRegistry.hpp"


DataTransform_Widget::DataTransform_Widget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataTransform_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    _registry = std::make_unique<TransformRegistry>();

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataTransform_Widget::_handleFeatureSelected);
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
    auto data_variant = _data_manager->getDataVariant(key);

    if (data_variant == std::nullopt) return;
    std::vector<std::string> operation_names = _registry->getOperationNamesForVariant(data_variant.value());

    std::cout << "Available Operations: \n";
    for (auto const & name: operation_names) {
        std::cout << name << "\n";
    }
    std::cout << std::endl;
}
