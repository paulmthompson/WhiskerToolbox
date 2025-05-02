

#include "DataTransform_Widget.hpp"

#include "ui_DataTransform_Widget.h"

#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "transforms/TransformRegistry.hpp"

#include "DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.hpp"


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

    _initializeParameterWidgetFactories();

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataTransform_Widget::_handleFeatureSelected);
    connect(ui->do_transform_button, &QPushButton::clicked, this, &DataTransform_Widget::_doTransform);
    connect(ui->operationComboBox, &QComboBox::currentIndexChanged, this, &DataTransform_Widget::_onOperationSelected);
}

DataTransform_Widget::~DataTransform_Widget() {
    delete ui;
}

void DataTransform_Widget::openWidget() {
    ui->feature_table_widget->populateTable();
    this->show();
}

void DataTransform_Widget::_initializeParameterWidgetFactories() {

    _parameterWidgetFactories["Calculate Area"] = nullptr;

    _parameterWidgetFactories["Threshold Event Detection"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new AnalogEventThreshold_Widget(parent);
    };

    /*
    parameterWidgetFactories_["Calculate Threshold"] = [](QWidget* parent) -> IParameterWidget* {
        return new ThresholdWidget(parent);
    };

    parameterWidgetFactories_["Smooth Data"] = [](QWidget* parent) -> IParameterWidget* {
        // return new SmoothingWidget(parent); // Example
        return nullptr; // Placeholder if not implemented
    };
    */
}


void DataTransform_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;

    auto key = feature.toStdString();
    auto data_variant = _data_manager->getDataVariant(key);

    if (data_variant == std::nullopt) return;
    std::vector<std::string> operation_names = _registry->getOperationNamesForVariant(data_variant.value());

    ui->operationComboBox->clear();

    if (operation_names.empty()) {
        ui->operationComboBox->addItem("No operations available");
        ui->operationComboBox->setEnabled(false);
        ui->do_transform_button->setEnabled(false);
    } else {
        for (std::string const & op_name: operation_names) {
            ui->operationComboBox->addItem(QString::fromStdString(op_name));
        }
        ui->operationComboBox->setEnabled(true);
        ui->do_transform_button->setEnabled(true);
        ui->operationComboBox->setCurrentIndex(0);
    }

    _currentSelectedDataVariant = data_variant.value();
}

void DataTransform_Widget::_onOperationSelected(int index) {
    _currentParameterWidget = nullptr;

    if (index < 0) {
        std::cout << "selected index is less than 0 and invalid: " << index << std::endl;
        _currentSelectedOperation = nullptr;
        ui->stackedWidget->setCurrentIndex(0);// Show default page
        return;
    }

    std::string const op_name = ui->operationComboBox->itemText(index).toStdString();

    _currentSelectedOperation = _registry->findOperationByName(op_name);
    std::cout << "Selecting operation " << op_name << std::endl;

    if (!_currentSelectedOperation) {
        std::cout << "Operation " << op_name << "not found in registry" << std::endl;
        ui->stackedWidget->setCurrentIndex(0);
        return;
    }

    _displayParameterWidget(op_name);
}

// Helper function to show the correct widget
void DataTransform_Widget::_displayParameterWidget(std::string const & op_name) {
    _currentParameterWidget = nullptr;
    QWidget * targetWidget = nullptr;

    // Find the factory function for this operation name
    auto factoryIt = _parameterWidgetFactories.find(op_name);

    if (factoryIt == _parameterWidgetFactories.end()) {
        std::cout << op_name << " does not appear in the factory registry" << std::endl;
        return;
    }

    if (!factoryIt->second) {
        std::cout << "Factory function not found for " << op_name << std::endl;
        return;
    }

    std::cout << "Calling factory for widget " << op_name << std::endl;

    std::function<TransformParameter_Widget *(QWidget *)> const factory = factoryIt->second;
    TransformParameter_Widget * newParamWidget = factory(ui->stackedWidget);// Create with parent

    if (newParamWidget) {
        std::cout << "Adding Widget" << std::endl;
        int const widgetIndex = ui->stackedWidget->addWidget(newParamWidget);
        targetWidget = newParamWidget;
        _currentParameterWidget = newParamWidget;// Set as active
    }

    if (targetWidget) {
        ui->stackedWidget->setCurrentWidget(targetWidget);
    } else {
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void DataTransform_Widget::_doTransform() {

    if (!_currentSelectedOperation) {
        std::cout << "Does not have operation" << std::endl;
        return;
    }

    std::unique_ptr<TransformParametersBase> params_owner_ptr;
    if (_currentParameterWidget) {// Check if a specific param widget is active
        params_owner_ptr = _currentParameterWidget->getParameters();
    } else {
        // No specific widget active. Maybe get defaults from operation? (Optional)
        // params_owner_ptr = currentSelectedOperation_->getDefaultParameters();
    }


    std::cout << "Executing '" << _currentSelectedOperation->getName() << "'..." << std::endl;
    // Pass non-owning raw pointer to the Qt-agnostic execute method
    auto result_any = _currentSelectedOperation->execute(
            _currentSelectedDataVariant,
            params_owner_ptr.get()// Pass raw pointer (nullptr if no params/widget)
    );
    // Process result_any...

    _data_manager->setData("Test", result_any);
}
