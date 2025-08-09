#include "DataManager_Widget.hpp"
#include "ui_DataManager_Widget.h"

#include "DataManager.hpp"
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

#include "AnalogTimeSeries_Widget/AnalogTimeSeries_Widget.hpp"
#include "DigitalEventSeries_Widget/DigitalEventSeries_Widget.hpp"
#include "DigitalIntervalSeries_Widget/DigitalIntervalSeries_Widget.hpp"
#include "Image_Widget/Image_Widget.hpp"
#include "Line_Widget/Line_Widget.hpp"
#include "Mask_Widget/Mask_Widget.hpp"
#include "NewDataWidget/NewDataWidget.hpp"
#include "OutputDirectoryWidget/OutputDirectoryWidget.hpp"
#include "Point_Widget/Point_Widget.hpp"
#include "Tensor_Widget/Tensor_Widget.hpp"

#include "TimeScrollBar/TimeScrollBar.hpp"

#include <QFileDialog>

DataManager_Widget::DataManager_Widget(
        std::shared_ptr<DataManager> data_manager,
        TimeScrollBar * time_scrollbar,
        QWidget * parent)
    : QScrollArea(parent),
      ui(new Ui::DataManager_Widget),
      _time_scrollbar{time_scrollbar},
      _data_manager{std::move(data_manager)} {
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
    ui->stackedWidget->addWidget(new Point_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Image_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Mask_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Line_Widget(_data_manager));
    ui->stackedWidget->addWidget(new AnalogTimeSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalIntervalSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new DigitalEventSeries_Widget(_data_manager));
    ui->stackedWidget->addWidget(new Tensor_Widget(_data_manager));

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
}

DataManager_Widget::~DataManager_Widget() {
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

            int const stacked_widget_index = 1;

            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto point_widget = dynamic_cast<Point_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            point_widget->setActiveKey(key);

            _current_data_callbacks.push_back(_data_manager->addCallbackToData(key, [point_widget]() {
                point_widget->updateTable();
            }));

            connect(point_widget, &Point_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            break;
        }
        case DM_DataType::Images: {

            int const stacked_widget_index = 2;

            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto image_widget = dynamic_cast<Image_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            image_widget->setActiveKey(key);

            connect(image_widget, &Image_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            break;
        }
        case DM_DataType::Mask: {

            int const stacked_widget_index = 3;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto mask_widget = dynamic_cast<Mask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            mask_widget->setActiveKey(key);

            connect(mask_widget, &Mask_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            break;
        }
        case DM_DataType::Line: {
            int const stacked_widget_index = 4;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto line_widget = dynamic_cast<Line_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (line_widget) {
                line_widget->setActiveKey(key);
                connect(line_widget, &Line_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            }
            break;
        }
        case DM_DataType::Analog: {

            int const stacked_widget_index = 5;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto analog_widget = dynamic_cast<AnalogTimeSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            analog_widget->setActiveKey(key);
            break;
        }
        case DM_DataType::DigitalInterval: {

            int const stacked_widget_index = 6;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            interval_widget->setActiveKey(key);

            connect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);

            break;
        }
        case DM_DataType::DigitalEvent: {


            int const stacked_widget_index = 7;
            ui->stackedWidget->setCurrentIndex(stacked_widget_index);
            auto event_widget = dynamic_cast<DigitalEventSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            event_widget->setActiveKey(key);

            connect(event_widget, &DigitalEventSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            break;
        }
        case DM_DataType::Tensor: {

            ui->stackedWidget->setCurrentIndex(8);
            dynamic_cast<Tensor_Widget *>(ui->stackedWidget->widget(8))->setActiveKey(key);
            break;
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

            int const stacked_widget_index = 1;

            auto point_widget = dynamic_cast<Point_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(point_widget, &Point_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            if (point_widget) {
                point_widget->removeCallbacks();
            }
            break;
        }
        case DM_DataType::Images: {

            int const stacked_widget_index = 2;

            auto image_widget = dynamic_cast<Image_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (image_widget) {
                disconnect(image_widget, &Image_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
                image_widget->removeCallbacks();
            }
            break;
        }
        case DM_DataType::Mask: {

            int const stacked_widget_index = 3;
            auto mask_widget = dynamic_cast<Mask_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (mask_widget) {
                disconnect(mask_widget, &Mask_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
                mask_widget->removeCallbacks();
            }
            break;
        }
        case DM_DataType::Line: {
            int const stacked_widget_index = 4;
            auto line_widget = dynamic_cast<Line_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            if (line_widget) {
                disconnect(line_widget, &Line_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
                line_widget->removeCallbacks();
            }
            break;
        }
        case DM_DataType::Analog: {
            int const stacked_widget_index = 5;
            break;
        }
        case DM_DataType::DigitalInterval: {

            int const stacked_widget_index = 6;
            auto interval_widget = dynamic_cast<DigitalIntervalSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(interval_widget, &DigitalIntervalSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            interval_widget->removeCallbacks();
            break;
        }
        case DM_DataType::DigitalEvent: {

            int const stacked_widget_index = 7;
            auto event_widget = dynamic_cast<DigitalEventSeries_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
            disconnect(event_widget, &DigitalEventSeries_Widget::frameSelected, this, &DataManager_Widget::_changeScrollbar);
            event_widget->removeCallbacks();
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
    } else if (type == "Mask") {

        auto const image_size = _data_manager->getData<MediaData>("media")->getImageSize();

        _data_manager->setData<MaskData>(key, time_key);
        _data_manager->getData<MaskData>(key)->setImageSize(image_size);

    } else if (type == "Line") {
        _data_manager->setData<LineData>(key, time_key);
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

void DataManager_Widget::_changeScrollbar(int frame_id) {

    auto active_feature = _highlighted_available_feature.toStdString();

    auto video_timeframe = _data_manager->getTime(TimeKey("time"));

    auto active_feature_timeframe_key = _data_manager->getTimeKey(active_feature);

    if (!active_feature_timeframe_key.empty()) {
        auto feature_timeframe = _data_manager->getTime(active_feature_timeframe_key);

        if (video_timeframe.get() != feature_timeframe.get()) {
            frame_id = feature_timeframe->getTimeAtIndex(TimeFrameIndex(frame_id));
            frame_id = video_timeframe->getIndexAtTime(frame_id).getValue();
        }
    }

    _time_scrollbar->changeScrollBarValue(frame_id);
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