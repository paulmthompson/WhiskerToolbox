#include "DataTransform_Widget.hpp"

#include "ui_DataTransform_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/TransformRegistry.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include "DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.hpp"
#include "DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.hpp"
#include "DataTransform_Widget/Lines/LineAngle_Widget/LineAngle_Widget.hpp"
#include "DataTransform_Widget/Lines/LineClip_Widget/LineClip_Widget.hpp"
#include "DataTransform_Widget/Lines/LineCurvature_Widget/LineCurvature_Widget.hpp"
#include "DataTransform_Widget/Lines/LineMinDist_Widget/LineMinDist_Widget.hpp"
#include "DataTransform_Widget/Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.hpp"
#include "DataTransform_Widget/Lines/LineResample_Widget/LineResample_Widget.hpp"
#include "DataTransform_Widget/Lines/LineSubsegment_Widget/LineSubsegment_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskArea_Widget/MaskArea_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskCentroid_Widget/MaskCentroid_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskToLine_Widget/MaskToLine_Widget.hpp"
#include "AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.hpp"

#include <QApplication>


DataTransform_Widget::DataTransform_Widget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataTransform_Widget),
      _data_manager{std::move(data_manager)} {
    ui->setupUi(this);

    // Set explicit size policy and minimum size
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setMinimumSize(250, 400);

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

    _parameterWidgetFactories["Calculate Area"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskArea_Widget(parent);
    };

    _parameterWidgetFactories["Calculate Mask Centroid"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskCentroid_Widget(parent);
    };

    _parameterWidgetFactories["Remove Small Connected Components"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskConnectedComponent_Widget(parent);
    };

    _parameterWidgetFactories["Fill Mask Holes"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskHoleFilling_Widget(parent);
    };

    _parameterWidgetFactories["Apply Median Filter"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskMedianFilter_Widget(parent);
    };

    _parameterWidgetFactories["Calculate Mask Principal Axis"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskPrincipalAxis_Widget(parent);
    };

    _parameterWidgetFactories["Skeletonize Mask"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new MaskSkeletonize_Widget(parent);
    };

    _parameterWidgetFactories["Threshold Event Detection"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new AnalogEventThreshold_Widget(parent);
    };

    _parameterWidgetFactories["Threshold Interval Detection"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new AnalogIntervalThreshold_Widget(parent);
    };

    _parameterWidgetFactories["Hilbert Phase"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new AnalogHilbertPhase_Widget(parent);
    };

    _parameterWidgetFactories["Scale and Normalize"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new AnalogScaling_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Calculate Line Angle"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new LineAngle_Widget(parent);
    };

    _parameterWidgetFactories["Calculate Line to Point Distance"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new LineMinDist_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Convert Mask to Line"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new MaskToLine_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Resample Line"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new LineResample_Widget(parent);
    };

    _parameterWidgetFactories["Calculate Line Curvature"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new LineCurvature_Widget(parent);
    };

    _parameterWidgetFactories["Extract Line Subsegment"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new LineSubsegment_Widget(parent);
    };

    _parameterWidgetFactories["Extract Point from Line"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new LinePointExtraction_Widget(parent);
    };

    _parameterWidgetFactories["Clip Line by Reference Line"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new LineClip_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Group Intervals"] = [](QWidget * parent) -> TransformParameter_Widget * {
        return new GroupIntervals_Widget(parent);
    };

    _parameterWidgetFactories["Filter"] = [](QWidget * parent)  -> TransformParameter_Widget * {
        return new AnalogFilter_Widget(parent);
    };
}


void DataTransform_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;

    auto key = feature.toStdString();
    auto data_variant = _data_manager->getDataVariant(key);

    if (data_variant == std::nullopt) return;
    std::vector<std::string> const operation_names = _registry->getOperationNamesForVariant(data_variant.value());

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

    // Update current parameter widget if it's a scaling widget
    if (_currentParameterWidget) {
        auto scalingWidget = dynamic_cast<AnalogScaling_Widget *>(_currentParameterWidget);
        if (scalingWidget) {
            scalingWidget->setCurrentDataKey(feature);
        }
    }

    // Update the output name based on the selected feature and current operation
    _updateOutputName();
}

void DataTransform_Widget::_onOperationSelected(int index) {
    _currentParameterWidget = nullptr;

    if (index < 0) {
        std::cout << "selected index is less than 0 and invalid: " << index << std::endl;
        _currentSelectedOperation = nullptr;

        // Clear all parameter widgets when no operation is selected
        while (ui->stackedWidget->count() > 1) {
            QWidget * widget = ui->stackedWidget->widget(1);
            ui->stackedWidget->removeWidget(widget);
            widget->deleteLater();
        }

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

    // Update the output name based on the selected operation and current feature
    _updateOutputName();
}

// Helper function to show the correct widget
void DataTransform_Widget::_displayParameterWidget(std::string const & op_name) {
    // Clear current parameter widget
    _currentParameterWidget = nullptr;

    // Remove all widgets from stacked widget except the default page (index 0)
    while (ui->stackedWidget->count() > 1) {
        QWidget * widget = ui->stackedWidget->widget(1);
        ui->stackedWidget->removeWidget(widget);
        widget->deleteLater();
    }

    // Find the factory function for this operation name
    auto factoryIt = _parameterWidgetFactories.find(op_name);

    if (factoryIt == _parameterWidgetFactories.end()) {
        std::cout << op_name << " does not appear in the factory registry" << std::endl;
        ui->stackedWidget->setCurrentIndex(0);
        return;
    }

    if (!factoryIt->second) {
        std::cout << "Factory function not found for " << op_name << std::endl;
        ui->stackedWidget->setCurrentIndex(0);
        return;
    }

    std::cout << "Calling factory for widget " << op_name << std::endl;

    std::function<TransformParameter_Widget *(QWidget *)> const factory = factoryIt->second;
    TransformParameter_Widget * newParamWidget = factory(ui->stackedWidget);// Create with parent

    if (newParamWidget) {
        std::cout << "Adding Widget" << std::endl;
        int const widgetIndex = ui->stackedWidget->addWidget(newParamWidget);
        _currentParameterWidget = newParamWidget;// Set as active

        // If this is a scaling widget, set the current data key
        auto scalingWidget = dynamic_cast<AnalogScaling_Widget *>(newParamWidget);
        if (scalingWidget && !_highlighted_available_feature.isEmpty()) {
            scalingWidget->setCurrentDataKey(_highlighted_available_feature);
        }

        ui->stackedWidget->setCurrentWidget(newParamWidget);
    } else {
        ui->stackedWidget->setCurrentIndex(0);
    }
}

void DataTransform_Widget::_updateProgress(int progress) {

    if (progress > _current_progress) {
        ui->transform_progress_bar->setValue(progress);
        ui->transform_progress_bar->setFormat("%p%");// Show percentage text
        ui->transform_progress_bar->repaint();       // Force immediate repaint
        QApplication::processEvents();               // Process all pending events to ensure UI updates
        _current_progress = progress;
    }
}

void DataTransform_Widget::_doTransform() {
    auto const new_data_key = ui->output_name_edit->text().toStdString();

    if (new_data_key.empty()) {
        std::cout << "Output name is empty" << std::endl;
        return;
    }

    if (!_currentSelectedOperation) {
        std::cout << "Does not have operation" << std::endl;
        return;
    }

    // Reset and show the progress bar
    ui->transform_progress_bar->setValue(0);
    _current_progress = 0;
    ui->transform_progress_bar->setFormat("%p%");
    ui->transform_progress_bar->setTextVisible(true);
    ui->transform_progress_bar->repaint();
    ui->do_transform_button->setEnabled(false);
    QApplication::processEvents();

    std::unique_ptr<TransformParametersBase> params_owner_ptr;
    if (_currentParameterWidget) {// Check if a specific param widget is active
        params_owner_ptr = _currentParameterWidget->getParameters();
    } else {
        // No specific widget active. Maybe get defaults from operation? (Optional)
        // params_owner_ptr = currentSelectedOperation_->getDefaultParameters();
    }

    std::cout << "Executing '" << _currentSelectedOperation->getName() << "'..." << std::endl;

    // Create a progress callback - use direct connection for immediate updates
    auto progressCallback = [this](int progress) {
        // Update directly from the UI thread to ensure immediate updates
        _updateProgress(progress);
    };

    // Pass non-owning raw pointer to the Qt-agnostic execute method with progress callback
    auto result_any = _currentSelectedOperation->execute(
            _currentSelectedDataVariant,
            params_owner_ptr.get(),
            progressCallback);

    _data_manager->setData(new_data_key, result_any);

    // Make sure new data is in the same temporal coordinate system as the input
    auto input_time_key = _data_manager->getTimeFrame(_highlighted_available_feature.toStdString());
    if (!input_time_key.empty()) {
        auto success = _data_manager->setTimeFrame(new_data_key, input_time_key);
        std::cout << "Time key "<< input_time_key << " for input feature: " 
                  << _highlighted_available_feature.toStdString() << " was "
                  << "set for new data: " << (success ? "Success" : "Failed") << std::endl;
    } else {
        std::cout << "No time key found for input feature: " << _highlighted_available_feature.toStdString() << std::endl;
    }   

    ui->transform_progress_bar->setValue(100);
    ui->do_transform_button->setEnabled(true);
}

QString DataTransform_Widget::_generateOutputName() const {
    // Return empty string if either feature or operation is not selected
    if (_highlighted_available_feature.isEmpty() || !_currentSelectedOperation) {
        return QString();
    }

    QString const inputKey = _highlighted_available_feature;
    QString transformName = QString::fromStdString(_currentSelectedOperation->getName());

    // Clean up the transform name for use in the output name:
    // - Convert to lowercase
    // - Replace spaces with underscores
    // - Remove common prefixes like "Calculate", "Extract", etc.
    transformName = transformName.toLower();
    transformName.replace(' ', '_');

    // Remove common operation prefixes to make names more concise
    QStringList const prefixesToRemove = {"calculate_", "extract_", "convert_", "threshold_"};
    for (QString const & prefix: prefixesToRemove) {
        if (transformName.startsWith(prefix)) {
            transformName = transformName.mid(prefix.length());
            break;
        }
    }

    // Combine input key and cleaned transform name
    return inputKey + "_" + transformName;
}

void DataTransform_Widget::_updateOutputName() {
    QString const outputName = _generateOutputName();
    if (!outputName.isEmpty()) {
        ui->output_name_edit->setText(outputName);
    }
}

QSize DataTransform_Widget::sizeHint() const {
    return QSize(350, 600);
}

QSize DataTransform_Widget::minimumSizeHint() const {
    return QSize(250, 400);
}
