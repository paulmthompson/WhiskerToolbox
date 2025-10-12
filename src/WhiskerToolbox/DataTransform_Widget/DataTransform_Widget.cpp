#include "DataTransform_Widget.hpp"

#include "ui_DataTransform_Widget.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/transforms/TransformRegistry.hpp"
#include "DataManager/transforms/TransformPipeline.hpp"
#include "DataManager/transforms/ParameterFactory.hpp"
#include "Feature_Table_Widget/Feature_Table_Widget.hpp"

#include "DataTransform_Widget/AnalogTimeSeries/AnalogEventThreshold_Widget/AnalogEventThreshold_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogHilbertPhase_Widget/AnalogHilbertPhase_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogIntervalThreshold_Widget/AnalogIntervalThreshold_Widget.hpp"
#include "DataTransform_Widget/AnalogTimeSeries/AnalogScaling_Widget/AnalogScaling_Widget.hpp"
#include "DataTransform_Widget/DigitalIntervalSeries/GroupIntervals_Widget/GroupIntervals_Widget.hpp"
#include "DataTransform_Widget/Lines/LineAngle_Widget/LineAngle_Widget.hpp"
#include "DataTransform_Widget/Lines/LineClip_Widget/LineClip_Widget.hpp"
#include "DataTransform_Widget/Lines/Line_Proximity_Grouping/LineProximityGrouping_Widget.hpp"
#include "DataTransform_Widget/Lines/Line_Kalman_Grouping/LineKalmanGrouping_Widget.hpp"
#include "DataTransform_Widget/Lines/LineCurvature_Widget/LineCurvature_Widget.hpp"
#include "DataTransform_Widget/Lines/LineMinDist_Widget/LineMinDist_Widget.hpp"
#include "DataTransform_Widget/Lines/LineAlignment_Widget/LineAlignment_Widget.hpp"
#include "DataTransform_Widget/Lines/LinePointExtraction_Widget/LinePointExtraction_Widget.hpp"
#include "DataTransform_Widget/Lines/LineResample_Widget/LineResample_Widget.hpp"
#include "DataTransform_Widget/Lines/LineSubsegment_Widget/LineSubsegment_Widget.hpp"
#include "DataTransform_Widget/Points/PointParticleFilter_Widget/PointParticleFilter_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskArea_Widget/MaskArea_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskCentroid_Widget/MaskCentroid_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskConnectedComponent_Widget/MaskConnectedComponent_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskHoleFilling_Widget/MaskHoleFilling_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskMedianFilter_Widget/MaskMedianFilter_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskPrincipalAxis_Widget/MaskPrincipalAxis_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskSkeletonize_Widget/MaskSkeletonize_Widget.hpp"
#include "DataTransform_Widget/Masks/MaskToLine_Widget/MaskToLine_Widget.hpp"
#include "AnalogTimeSeries/AnalogFilter_Widget/AnalogFilter_Widget.hpp"
#include "Media/WhiskerTracing_Widget/WhiskerTracing_Widget.hpp"

#include <QApplication>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QFont>
#include <QSplitter>
#include <QScrollBar>
#include <QTimer>


DataTransform_Widget::DataTransform_Widget(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QScrollArea(parent),
      ui(new Ui::DataTransform_Widget),
      _data_manager{std::move(data_manager)},
      _jsonPipelineGroup(nullptr),
      _loadJsonButton(nullptr),
      _jsonTextEdit(nullptr),
      _jsonStatusLabel(nullptr),
      _executeJsonButton(nullptr),
      _pipelineProgressBar(nullptr),
      _savedScrollPosition(0),
      _preventScrolling(false) {
    ui->setupUi(this);

    // Set explicit size policy and minimum size - increased for better visibility
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setMinimumSize(350, 700);

    // Configure scroll area properties - enable scrolling for JSON section
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Disable automatic scrolling to focused widgets
    setFocusPolicy(Qt::NoFocus);

    _registry = std::make_unique<TransformRegistry>();
    
    // Initialize pipeline with registry and data manager
    _pipeline = std::make_unique<TransformPipeline>(_data_manager.get(), _registry.get());
    
    // Initialize parameter factory
    ParameterFactory::getInstance().initializeDefaultSetters();

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    _initializeParameterWidgetFactories();
    _setupJsonPipelineUI();

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

    _parameterWidgetFactories["Line Alignment to Bright Features"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new LineAlignment_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Convert Mask To Line"] = [this](QWidget * parent) -> TransformParameter_Widget * {
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

    _parameterWidgetFactories["Group Lines by Proximity"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new LineProximityGrouping_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Group Lines using Kalman Filtering"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new LineKalmanGrouping_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Track Points Through Masks (Particle Filter)"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new PointParticleFilter_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
    };

    _parameterWidgetFactories["Filter"] = [](QWidget * parent)  -> TransformParameter_Widget * {
        return new AnalogFilter_Widget(parent);
    };

    _parameterWidgetFactories["Whisker Tracing"] = [this](QWidget * parent) -> TransformParameter_Widget * {
        auto widget = new WhiskerTracing_Widget(parent);
        widget->setDataManager(_data_manager);
        return widget;
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

        // Set size policy for dynamic resizing without scrollbars
        newParamWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        newParamWidget->setMaximumWidth(ui->stackedWidget->width());

        // Ensure no scrollbars appear
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        int const widgetIndex = ui->stackedWidget->addWidget(newParamWidget);
        Q_UNUSED(widgetIndex) // Suppress the warning about unused variable
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
        // Store current position before updating progress
        int currentPos = verticalScrollBar()->value();

        ui->transform_progress_bar->setValue(progress);
        ui->transform_progress_bar->setFormat("%p%");// Show percentage text
        ui->transform_progress_bar->repaint();       // Force immediate repaint
        QApplication::processEvents();               // Process all pending events to ensure UI updates

        // Restore position if we're preventing scrolling
        if (_preventScrolling) {
            verticalScrollBar()->setValue(currentPos);
        }

        _current_progress = progress;
    }
}

void DataTransform_Widget::_doTransform() {
    // Save current scroll position before starting transform
    _savedScrollPosition = verticalScrollBar()->value();
    _preventScrolling = true;

    auto const new_data_key = ui->output_name_edit->text().toStdString();

    if (new_data_key.empty()) {
        std::cout << "Output name is empty" << std::endl;
        _preventScrolling = false;
        return;
    }

    if (!_currentSelectedOperation) {
        std::cout << "Does not have operation" << std::endl;
        _preventScrolling = false;
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

    // Restore scroll position after UI updates
    verticalScrollBar()->setValue(_savedScrollPosition);

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


    auto input_time_key = _data_manager->getTimeKey(_highlighted_available_feature.toStdString());
    _data_manager->setData(new_data_key, result_any, input_time_key);

    ui->transform_progress_bar->setValue(100);
    ui->do_transform_button->setEnabled(true);

    // Restore scroll position after completion and re-enable scrolling
    verticalScrollBar()->setValue(_savedScrollPosition);
    _preventScrolling = false;
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

void DataTransform_Widget::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);

    // Ensure content widget fills the scroll area completely
    if (widget()) {
        widget()->resize(viewport()->size());
    }

    // Update the width of components inside the stackedWidget
    if (_currentParameterWidget) {
        _currentParameterWidget->setMaximumWidth(ui->stackedWidget->width());

        // Trigger layout update
        _currentParameterWidget->updateGeometry();
    }

    // Update JSON text editor width if it exists
    if (_jsonTextEdit) {
        _jsonTextEdit->setMaximumWidth(width() - 40); // Account for margins
    }

    // Ensure the stackedWidget is properly sized
    ui->stackedWidget->updateGeometry();

    // Maintain scroll position during resize if preventing scrolling
    if (_preventScrolling) {
        QTimer::singleShot(0, [this]() {
            verticalScrollBar()->setValue(_savedScrollPosition);
        });
    }
}

QSize DataTransform_Widget::sizeHint() const {
    return QSize(400, 900);
}

QSize DataTransform_Widget::minimumSizeHint() const {
    return QSize(350, 700);
}

void DataTransform_Widget::_setupJsonPipelineUI() {
    // Create the JSON pipeline group box
    _jsonPipelineGroup = new QGroupBox("JSON Pipeline", this);
    _jsonPipelineGroup->setCheckable(true);
    _jsonPipelineGroup->setChecked(false); // Start collapsed
    _jsonPipelineGroup->setMinimumHeight(50); // Smaller when collapsed
    
    auto jsonLayout = new QVBoxLayout(_jsonPipelineGroup);
    
    // JSON file loading section
    auto jsonButtonLayout = new QHBoxLayout();
    _loadJsonButton = new QPushButton("Load JSON Pipeline...", this);
    _jsonStatusLabel = new QLabel("No JSON pipeline loaded", this);
    _jsonStatusLabel->setStyleSheet("color: gray;");
    
    jsonButtonLayout->addWidget(_loadJsonButton);
    jsonButtonLayout->addWidget(_jsonStatusLabel, 1);
    jsonLayout->addLayout(jsonButtonLayout);
    
    // JSON text editor
    _jsonTextEdit = new QTextEdit(this);
    _jsonTextEdit->setPlaceholderText(
        "JSON pipeline configuration will appear here...\n\n"
        "Example:\n"
        "{\n"
        "  \"metadata\": {\n"
        "    \"name\": \"My Pipeline\",\n"
        "    \"version\": \"1.0\"\n"
        "  },\n"
        "  \"steps\": [\n"
        "    {\n"
        "      \"step_id\": \"step1\",\n"
        "      \"transform_name\": \"Line Alignment\",\n"
        "      \"input_key\": \"whisker_trace\",\n"
        "      \"output_key\": \"aligned_whisker\",\n"
        "      \"phase\": 0,\n"
        "      \"parameters\": {\n"
        "        \"width\": 25\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}"
    );
    
    // Set monospace font for JSON
    QFont monoFont("Consolas, Monaco, monospace");
    monoFont.setPointSize(9);
    _jsonTextEdit->setFont(monoFont);
    _jsonTextEdit->setMinimumHeight(200);
    
    jsonLayout->addWidget(_jsonTextEdit);
    
    // Pipeline execution section
    auto executeLayout = new QHBoxLayout();
    _executeJsonButton = new QPushButton("Execute Pipeline", this);
    _executeJsonButton->setEnabled(false);
    
    _pipelineProgressBar = new QProgressBar(this);
    _pipelineProgressBar->setVisible(false);
    _pipelineProgressBar->setTextVisible(true);
    
    executeLayout->addWidget(_executeJsonButton);
    executeLayout->addWidget(_pipelineProgressBar, 1);
    jsonLayout->addLayout(executeLayout);
    
    // Add the JSON pipeline group to the main layout
    // Get the scroll area's widget and its layout
    auto* scrollWidget = widget();
    if (scrollWidget) {
        auto* mainLayout = qobject_cast<QVBoxLayout*>(scrollWidget->layout());
        if (mainLayout) {
            // Insert the JSON pipeline group before the last spacer
            int spacerIndex = mainLayout->count() - 1;
            mainLayout->insertWidget(spacerIndex, _jsonPipelineGroup);
        }
    }
    
    // Connect signals
    connect(_loadJsonButton, &QPushButton::clicked, this, &DataTransform_Widget::_loadJsonPipeline);
    connect(_jsonTextEdit, &QTextEdit::textChanged, this, &DataTransform_Widget::_onJsonTextChanged);
    connect(_executeJsonButton, &QPushButton::clicked, this, &DataTransform_Widget::_executeJsonPipeline);
    
    // Connect group box toggle to adjust size
    connect(_jsonPipelineGroup, &QGroupBox::toggled, this, [this](bool checked) {
        if (checked) {
            _jsonPipelineGroup->setMinimumHeight(350);
        } else {
            _jsonPipelineGroup->setMinimumHeight(50);
        }
        updateGeometry();
    });
}

void DataTransform_Widget::_loadJsonPipeline() {
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "Load JSON Pipeline",
        "",
        "JSON Files (*.json);;All Files (*)"
    );
    
    if (!fileName.isEmpty()) {
        _updateJsonDisplay(fileName);
    }
}

void DataTransform_Widget::_updateJsonDisplay(const QString& jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + jsonFilePath);
        return;
    }
    
    QTextStream in(&file);
    QString jsonContent = in.readAll();
    file.close();
    
    // Update the text editor
    _jsonTextEdit->setPlainText(jsonContent);
    
    // Update status and file path
    _currentJsonFile = jsonFilePath;
    QFileInfo fileInfo(jsonFilePath);
    _jsonStatusLabel->setText("Loaded: " + fileInfo.fileName());
    _jsonStatusLabel->setStyleSheet("color: green;");
    
    // Validate and update UI
    _validateJsonSyntax();
}

void DataTransform_Widget::_onJsonTextChanged() {
    _validateJsonSyntax();
}

void DataTransform_Widget::_validateJsonSyntax() {
    QString jsonText = _getCurrentJsonContent();
    if (jsonText.isEmpty()) {
        _jsonStatusLabel->setText("No JSON content");
        _jsonStatusLabel->setStyleSheet("color: gray;");
        _executeJsonButton->setEnabled(false);
        return;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        _jsonStatusLabel->setText("JSON Error: " + error.errorString());
        _jsonStatusLabel->setStyleSheet("color: red;");
        _executeJsonButton->setEnabled(false);
    } else {
        // Try to load the pipeline to validate it
        nlohmann::json config;
        try {
            config = nlohmann::json::parse(jsonText.toStdString());
            
            if (_pipeline->loadFromJson(config)) {
                auto validation_errors = _pipeline->validate();
                if (validation_errors.empty()) {
                    _jsonStatusLabel->setText("Valid pipeline (" + QString::number(_pipeline->getSteps().size()) + " steps)");
                    _jsonStatusLabel->setStyleSheet("color: green;");
                    _executeJsonButton->setEnabled(true);
                } else {
                    QString errorText = "Validation errors: ";
                    for (size_t i = 0; i < validation_errors.size() && i < 2; ++i) {
                        if (i > 0) errorText += "; ";
                        errorText += QString::fromStdString(validation_errors[i]);
                    }
                    if (validation_errors.size() > 2) {
                        errorText += "...";
                    }
                    _jsonStatusLabel->setText(errorText);
                    _jsonStatusLabel->setStyleSheet("color: red;");
                    _executeJsonButton->setEnabled(false);
                }
            } else {
                _jsonStatusLabel->setText("Invalid pipeline configuration");
                _jsonStatusLabel->setStyleSheet("color: red;");
                _executeJsonButton->setEnabled(false);
            }
        } catch (std::exception const& e) {
            _jsonStatusLabel->setText("Pipeline error: " + QString::fromStdString(e.what()));
            _jsonStatusLabel->setStyleSheet("color: red;");
            _executeJsonButton->setEnabled(false);
        }
    }
}

void DataTransform_Widget::_executeJsonPipeline() {
    if (!_pipeline) {
        QMessageBox::warning(this, "Error", "No pipeline available");
        return;
    }
    
    // Save scroll position before starting pipeline
    _savedScrollPosition = verticalScrollBar()->value();
    _preventScrolling = true;

    // Load the current JSON content
    QString jsonText = _getCurrentJsonContent();
    nlohmann::json config;
    try {
        config = nlohmann::json::parse(jsonText.toStdString());
    } catch (std::exception const& e) {
        QMessageBox::warning(this, "JSON Error", "Failed to parse JSON: " + QString::fromStdString(e.what()));
        return;
    }
    
    if (!_pipeline->loadFromJson(config)) {
        QMessageBox::warning(this, "Pipeline Error", "Failed to load pipeline configuration");
        return;
    }
    
    // Setup progress tracking
    _pipelineProgressBar->setVisible(true);
    _pipelineProgressBar->setValue(0);
    _executeJsonButton->setEnabled(false);
    
    // Execute the pipeline with progress callback
    auto progressCallback = [this](int step_index, std::string const& step_name, int step_progress, int overall_progress) {
        _pipelineProgressBar->setValue(overall_progress);
        if (step_index >= 0) {
            _pipelineProgressBar->setFormat(QString("Step %1 (%2): %3%").arg(step_index).arg(QString::fromStdString(step_name)).arg(step_progress));
        } else {
            _pipelineProgressBar->setFormat(QString("%1: %2%").arg(QString::fromStdString(step_name)).arg(overall_progress));
        }
        QApplication::processEvents();
    };
    
    auto result = _pipeline->execute(progressCallback);
    
    // Handle results
    if (result.success) {
        _pipelineProgressBar->setValue(100);
        _pipelineProgressBar->setFormat("Pipeline completed successfully!");
        
        QMessageBox::information(this, "Success", 
            QString("Pipeline completed successfully!\n"
                   "Steps completed: %1/%2\n"
                   "Execution time: %3 ms")
                   .arg(result.steps_completed)
                   .arg(result.total_steps)
                   .arg(result.total_execution_time_ms, 0, 'f', 1));
                   
        // Refresh the feature table to show new data
        ui->feature_table_widget->populateTable();
    } else {
        _pipelineProgressBar->setFormat("Pipeline failed");
        _pipelineProgressBar->setStyleSheet("QProgressBar::chunk { background-color: red; }");
        
        QString errorDetails;
        for (size_t i = 0; i < result.step_results.size(); ++i) {
            auto const& step_result = result.step_results[i];
            if (!step_result.success) {
                errorDetails += QString("Step %1: %2\n").arg(i).arg(QString::fromStdString(step_result.error_message));
            }
        }
        
        QMessageBox::warning(this, "Pipeline Failed", 
            QString("Pipeline execution failed:\n%1\n\nSteps completed: %2/%3")
                   .arg(QString::fromStdString(result.error_message))
                   .arg(result.steps_completed)
                   .arg(result.total_steps) + 
                   (errorDetails.isEmpty() ? "" : "\n\nStep details:\n" + errorDetails));
    }
    
    _executeJsonButton->setEnabled(true);

    // Restore scroll position and re-enable scrolling
    verticalScrollBar()->setValue(_savedScrollPosition);
    _preventScrolling = false;
}

QString DataTransform_Widget::_getCurrentJsonContent() const {
    return _jsonTextEdit ? _jsonTextEdit->toPlainText() : QString();
}
