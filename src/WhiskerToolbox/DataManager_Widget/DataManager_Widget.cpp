#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include "DataManager.hpp"
#include "DataManagerWidgetState.hpp"
#include "EditorState/SelectionContext.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Media/Media_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include "CoreGeometry/ImageSize.hpp"

//https://stackoverflow.com/questions/72533139/libtorch-errors-when-used-with-qt-opencv-and-point-cloud-library
#undef slots
#include "DataManager/Tensors/Tensor_Data.hpp"
#define slots Q_SLOTS

#include "NewDataWidget/NewDataWidget.hpp"
#include "OutputDirectoryWidget/OutputDirectoryWidget.hpp"

#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QResizeEvent>
#include <QTableWidgetItem>

DataManager_Widget::DataManager_Widget(
        std::shared_ptr<DataManager> data_manager,
        EditorRegistry * editor_registry,
        QWidget * parent)
    : QScrollArea(parent),
      ui(new Ui::DataManager_Widget),
      _data_manager{std::move(data_manager)},
      _editor_registry{editor_registry} {
    ui->setupUi(this);

    // Set explicit size policy and minimum size
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setMinimumSize(250, 400);

    // Configure scroll area properties
    setWidgetResizable(true);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Ensure the content widget has the correct size policy
    ui->scrollAreaWidgetContents->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    ui->feature_table_widget->setColumns({"Feature", "Type", "Clock"});
    ui->feature_table_widget->setDataManager(_data_manager);

    // Ensure the feature table doesn't expand beyond available width
    ui->feature_table_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Initialize stacked widgets

    // Setup collapsible sections
    ui->output_dir_section->autoSetContentLayout();
    ui->output_dir_section->setTitle("Output Directory");

    ui->new_data_section->autoSetContentLayout();
    ui->new_data_section->setTitle("Create New Data");

    // Set the DataManager for the NewDataWidget that was created from the UI file
    ui->new_data_widget->setDataManager(_data_manager);

    // Configure size policies for child widgets to ensure they fit within container
    ui->output_dir_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->new_data_widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->selected_feature_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    ui->stackedWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    connect(ui->output_dir_widget, &OutputDirectoryWidget::dirChanged, this, &DataManager_Widget::_changeOutputDir);
    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataManager_Widget::_handleFeatureSelected);
    connect(ui->new_data_widget, &NewDataWidget::createNewData, this, &DataManager_Widget::_createNewData);

    // === Phase 2.3: Editor State Integration ===
    // Initialize state and register with EditorRegistry for serialization and inter-widget communication

    _state = std::make_shared<DataManagerWidgetState>();

    if (_editor_registry) {
        _editor_registry->registerState(_state);
        _selection_context = _editor_registry->selectionContext();

        // Connect feature table selection to state
        // When user selects a feature in the table, update the state
        connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected,
                this, [this](QString const & key) {
            _state->setSelectedDataKey(key);
        });

        // Connect state changes to SelectionContext for inter-widget communication
        // When state's selected data key changes, notify SelectionContext
        connect(_state.get(), &DataManagerWidgetState::selectedDataKeyChanged,
                this, [this](QString const & key) {
            if (_selection_context) {
                SelectionSource source{EditorInstanceId(_state->getInstanceId()), QStringLiteral("feature_table")};
                _selection_context->setSelectedData(SelectedDataKey(key), source);
            }
        });
    }
}

DataManager_Widget::~DataManager_Widget() {
    // Unregister state from EditorRegistry when widget is destroyed
    if (_editor_registry && _state) {
        _editor_registry->unregisterState(EditorInstanceId(_state->getInstanceId()));
    }
    delete ui;
}

void DataManager_Widget::openWidget() {
    ui->feature_table_widget->populateTable();
    // Refresh timeframes when opening the widget
    if (ui->new_data_widget) {
        ui->new_data_widget->populateTimeframes();
    }
    this->show();
}

void DataManager_Widget::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
}

void DataManager_Widget::clearFeatureSelection() {
    // Disable the currently selected feature if any
    if (!_highlighted_available_feature.isEmpty()) {
        _disablePreviousFeature(_highlighted_available_feature);
    }

    // Clear the highlighted feature
    _highlighted_available_feature.clear();

    // Reset the feature label to show no selection
    ui->selected_feature_label->setText("No Feature Selected");

    // Switch to the blank page in the stacked widget
    ui->stackedWidget->setCurrentIndex(0);// Index 0 is the blank widget
}

void DataManager_Widget::_handleFeatureSelected(QString const & feature) {
    // Disable the previously selected feature before switching to the new one
    if (!_highlighted_available_feature.isEmpty() && _highlighted_available_feature != feature) {
        _disablePreviousFeature(_highlighted_available_feature);
    }

    _highlighted_available_feature = feature;

    // Update the feature label to show the selected feature name
    ui->selected_feature_label->setText(feature);

    auto key = feature.toStdString();

    auto feature_type = _data_manager->getType(feature.toStdString());

    switch (feature_type) {
        case DM_DataType::Points: {
            break;
        }
        case DM_DataType::Images: {

            break;
        }
        case DM_DataType::Mask: {

            break;
        }
        case DM_DataType::Line: {

        }
        case DM_DataType::Analog: {

        }
        case DM_DataType::DigitalInterval: {
            break;
        }
        case DM_DataType::DigitalEvent: {
           
            break;
        }
        case DM_DataType::Tensor: {
        }
        case DM_DataType::Unknown: {
            std::cout << "Unsupported feature type" << std::endl;
            break;
        }
        default: {
            std::cout << "You shouldn't be here" << std::endl;
        }
    }
}

void DataManager_Widget::_disablePreviousFeature(QString const & feature) {

    auto key = feature.toStdString();

    for (auto callback: _current_data_callbacks) {
        _data_manager->removeCallbackFromData(key, callback);
    }

    _current_data_callbacks.clear();

    auto feature_type = _data_manager->getType(feature.toStdString());

    switch (feature_type) {
        case DM_DataType::Points: {

            break;
        }
        case DM_DataType::Images: {

            break;
        }
        case DM_DataType::Mask: {

            break;
        }
        case DM_DataType::Line: {
            break;
        }
        case DM_DataType::Analog: {
            int const stacked_widget_index = 5;
            break;
        }
        case DM_DataType::DigitalInterval: {
            break;
        }
        case DM_DataType::DigitalEvent: {

            break;
        }
        case DM_DataType::Tensor: {
            int const stacked_widget_index = 8;
            break;
        }
        case DM_DataType::Unknown: {
            std::cout << "Unsupported feature type" << std::endl;
            break;
        }
    }
}

void DataManager_Widget::_changeOutputDir(QString dir_name) {


    if (dir_name.isEmpty()) {
        return;
    }

    _data_manager->setOutputPath(dir_name.toStdString());
    ui->output_dir_widget->setDirLabel(dir_name);
}

void DataManager_Widget::_createNewData(std::string key, std::string type, std::string timeframe_key) {

    if (key.empty()) {
        return;
    }

    auto time_key = TimeKey("time");
    if (!timeframe_key.empty()) {
        time_key = TimeKey(timeframe_key);
    }

    if (type == "Point") {

        _data_manager->setData<PointData>(key, time_key);

        auto const image_size = _data_manager->getData<MediaData>("media")->getImageSize();
        _data_manager->getData<PointData>(key)->setImageSize(image_size);

    } else if (type == "Mask") {

        _data_manager->setData<MaskData>(key, time_key);

        auto const image_size = _data_manager->getData<MediaData>("media")->getImageSize();
        _data_manager->getData<MaskData>(key)->setImageSize(image_size);

    } else if (type == "Line") {

        _data_manager->setData<LineData>(key, time_key);

        auto const image_size = _data_manager->getData<MediaData>("media")->getImageSize();
        _data_manager->getData<LineData>(key)->setImageSize(image_size);

    } else if (type == "Analog Time Series") {
        _data_manager->setData<AnalogTimeSeries>(key, time_key);
    } else if (type == "Interval") {
        _data_manager->setData<DigitalIntervalSeries>(key, time_key);
    } else if (type == "Event") {
        _data_manager->setData<DigitalEventSeries>(key, time_key);
    } else if (type == "Tensor") {
        _data_manager->setData<TensorData>(key, time_key);
    } else {
        std::cout << "Unsupported data type" << std::endl;
        return;
    }
}

void DataManager_Widget::_deleteData(QString const & feature) {
    if (feature.isEmpty()) {
        return;
    }

    // If the currently highlighted feature is being deleted, clear the selection first
    if (_highlighted_available_feature == feature) {
        clearFeatureSelection();
    }

    // Delete the data from the DataManager
    std::string const key = feature.toStdString();
    if (_data_manager->deleteData(key)) {
        // Refresh the feature table to reflect the deletion
        ui->feature_table_widget->populateTable();
        
        // If we had a feature selected for restoration, clear it since it's no longer valid
        if (!_highlighted_available_feature.isEmpty()) {
            _highlighted_available_feature.clear();
        }
    }
}

void DataManager_Widget::contextMenuEvent(QContextMenuEvent* event) {
    _showContextMenu(event->globalPos());
}

void DataManager_Widget::_showContextMenu(QPoint const & pos) {
    // Use the currently highlighted feature from the feature table widget
    QString const feature = ui->feature_table_widget->getHighlightedFeature();
    if (feature.isEmpty()) {
        return;
    }

    // Create context menu
    QMenu contextMenu(this);
    
    // Add delete action
    QAction * deleteAction = contextMenu.addAction("Delete");
    deleteAction->setIcon(QIcon::fromTheme("edit-delete"));
    
    // Show the menu and handle the action
    QAction * selectedAction = contextMenu.exec(pos);
    if (selectedAction == deleteAction) {
        _deleteData(feature);
    }
}

void DataManager_Widget::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);

    // Ensure content widget fills the scroll area completely
    if (widget()) {
        widget()->resize(viewport()->size());
    }

    // Force layout update to ensure proper sizing of child widgets
    if (ui->scrollAreaWidgetContents->layout()) {
        ui->scrollAreaWidgetContents->layout()->invalidate();
        ui->scrollAreaWidgetContents->layout()->activate();
    }

    // Update child widgets to ensure they fit within the available width
    int const availableWidth = viewport()->width() - 20; // Account for margins

    // Update the feature table width
    if (ui->feature_table_widget) {
        ui->feature_table_widget->setMaximumWidth(availableWidth);
        ui->feature_table_widget->updateGeometry();
    }

    // Update the sections
    if (ui->output_dir_section) {
        ui->output_dir_section->setMaximumWidth(availableWidth);
        ui->output_dir_section->updateGeometry();
    }

    if (ui->new_data_section) {
        ui->new_data_section->setMaximumWidth(availableWidth);
        ui->new_data_section->updateGeometry();
    }

    // Update the stacked widget
    if (ui->stackedWidget) {
        ui->stackedWidget->setMaximumWidth(availableWidth);
        ui->stackedWidget->updateGeometry();

        // Update the current widget in the stack
        QWidget* currentWidget = ui->stackedWidget->currentWidget();
        if (currentWidget) {
            currentWidget->setMaximumWidth(availableWidth);
            currentWidget->updateGeometry();
        }
    }
}

void DataManager_Widget::showEvent(QShowEvent* event) {
    QScrollArea::showEvent(event);

    // Delay the resize slightly to ensure it happens after the widget is visible
    QTimer::singleShot(0, this, [this]() {
        if (widget() && viewport()) {
            // Resize the content widget to match the viewport
            widget()->resize(viewport()->size());

            // Force layout update
            if (ui->scrollAreaWidgetContents->layout()) {
                ui->scrollAreaWidgetContents->layout()->invalidate();
                ui->scrollAreaWidgetContents->layout()->activate();
            }

            // Update geometry
            widget()->updateGeometry();
        }
    });
}

QSize DataManager_Widget::sizeHint() const {
    return QSize(350, 600);
}

QSize DataManager_Widget::minimumSizeHint() const {
    return QSize(250, 400);
}