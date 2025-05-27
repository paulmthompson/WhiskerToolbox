
#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "DataManager.hpp"

#include "Feature_Table_Widget/Feature_Table_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "DigitalInterval/IntervalViewer_Widget.hpp"
#include "DigitalEvent/EventViewer_Widget.hpp"

#include <QTableWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <iostream>

DataViewer_Widget::DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     TimeScrollBar * time_scrollbar,
                                     MainWindow * main_window,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      _main_window{main_window},
      ui(new Ui::DataViewer_Widget) {

    ui->setupUi(this);

    ui->feature_table_widget->setColumns({"Feature", "Enabled", "Type", "Clock"});
    ui->feature_table_widget->setTypeFilter({DM_DataType::Analog, DM_DataType::DigitalEvent, DM_DataType::DigitalInterval});

    ui->feature_table_widget->setDataManager(_data_manager);

    connect(ui->feature_table_widget, &Feature_Table_Widget::featureSelected, this, &DataViewer_Widget::_handleFeatureSelected);
    connect(ui->feature_table_widget, &Feature_Table_Widget::addFeature, this, [this](QString const & feature) {
        DataViewer_Widget::_addFeatureToModel(feature, true);
    });
    connect(ui->feature_table_widget, &Feature_Table_Widget::removeFeature, this, [this](QString const & feature) {
        DataViewer_Widget::_addFeatureToModel(feature, false);
    });

    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleXAxisSamplesChanged);

    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged, this, &DataViewer_Widget::_updateGlobalScale);


    connect(time_scrollbar, &TimeScrollBar::timeChanged, this, &DataViewer_Widget::_updatePlot);

    //We should alwasy get the master clock because we plot
    // Check for master clock
    auto time_keys = _data_manager->getTimeFrameKeys();
    // if timekeys doesn't have master, we should throw an error
    if (std::find(time_keys.begin(), time_keys.end(), "master") == time_keys.end()) {
        std::cout << "No master clock found in DataManager" << std::endl;
        _time_frame = _data_manager->getTime("time");
    } else {
        _time_frame = _data_manager->getTime("master");
    }

    std::cout << "Setting GL limit to " << _time_frame->getTotalFrameCount() << std::endl;
    ui->openGLWidget->setXLimit(_time_frame->getTotalFrameCount());

    // Setup stacked widget with data-type specific viewers
    auto analog_widget = new AnalogViewer_Widget(_data_manager, ui->openGLWidget);
    auto interval_widget = new IntervalViewer_Widget(_data_manager, ui->openGLWidget);
    auto event_widget = new EventViewer_Widget(_data_manager, ui->openGLWidget);
    
    ui->stackedWidget->addWidget(analog_widget);
    ui->stackedWidget->addWidget(interval_widget);
    ui->stackedWidget->addWidget(event_widget);
    
    // Connect color change signals from sub-widgets
    connect(analog_widget, &AnalogViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);
    connect(interval_widget, &IntervalViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);
    connect(event_widget, &EventViewer_Widget::colorChanged,
            this, &DataViewer_Widget::_handleColorChanged);
    
    // Connect mouse hover signal from OpenGL widget
    connect(ui->openGLWidget, &OpenGLWidget::mouseHover,
            this, &DataViewer_Widget::_updateCoordinateDisplay);
}

DataViewer_Widget::~DataViewer_Widget() {
    delete ui;
}

void DataViewer_Widget::openWidget() {
    std::cout << "DataViewer Widget Opened" << std::endl;
    ui->feature_table_widget->populateTable();
    this->show();

    _updateLabels();
}

void DataViewer_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;
}

void DataViewer_Widget::_updatePlot(int time) {
    //std::cout << "Time is " << time;
    time = _data_manager->getTime("time")->getTimeAtIndex(time);
    //std::cout << ""
    ui->openGLWidget->updateCanvas(time);

    _updateLabels();
}


void DataViewer_Widget::_addFeatureToModel(QString const & feature, bool enabled) {

    if (enabled) {
        _plotSelectedFeature(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void DataViewer_Widget::_plotSelectedFeature(std::string const & key) {

    auto color = ui->feature_table_widget->getFeatureColor(key);

    if (_data_manager->getType(key) == DM_DataType::Analog) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);

    } else if (_data_manager->getType(key) == DM_DataType::DigitalEvent) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);
    } else if (_data_manager->getType(key) == DM_DataType::DigitalInterval) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}

void DataViewer_Widget::_removeSelectedFeature(std::string const & key) {
    if (_data_manager->getType(key) == DM_DataType::Analog) {
        ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (_data_manager->getType(key) == DM_DataType::DigitalEvent) {
        ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (_data_manager->getType(key) == DM_DataType::DigitalInterval) {
        ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported" << std::endl;
    }
}

void DataViewer_Widget::_handleFeatureSelected(QString const & feature) {
    _highlighted_available_feature = feature;
    
    // Switch stacked widget based on data type
    auto const type = _data_manager->getType(feature.toStdString());
    auto key = feature.toStdString();

    if (type == DM_DataType::Analog) {
        int const stacked_widget_index = 1;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto analog_widget = dynamic_cast<AnalogViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        analog_widget->setActiveKey(key);
        std::cout << "Selected Analog Time Series: " << key << std::endl;

    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 2;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<IntervalViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        interval_widget->setActiveKey(key);
        std::cout << "Selected Digital Interval Series: " << key << std::endl;

    } else if (type == DM_DataType::DigitalEvent) {
        int const stacked_widget_index = 3;
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto event_widget = dynamic_cast<EventViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        event_widget->setActiveKey(key);
        std::cout << "Selected Digital Event Series: " << key << std::endl;

    } else {
        ui->stackedWidget->setCurrentIndex(0);
        std::cout << "Unsupported feature type for detailed view" << std::endl;
    }
}

void DataViewer_Widget::_handleXAxisSamplesChanged(int value) {
    // Use setRangeWidth for spinbox changes (absolute value)
    std::cout << "Spinbox requested range width: " << value << std::endl;
    int64_t const actual_range = ui->openGLWidget->setRangeWidth(static_cast<int64_t>(value));
    std::cout << "Actual range width achieved: " << actual_range << std::endl;
    
    // Update the spinbox with the actual range width achieved (in case it was clamped)
    if (actual_range != value) {
        std::cout << "Range was clamped, updating spinbox to: " << actual_range << std::endl;
        updateXAxisSamples(static_cast<int>(actual_range));
    }
}

void DataViewer_Widget::updateXAxisSamples(int value) {
    ui->x_axis_samples->blockSignals(true);
    ui->x_axis_samples->setValue(value);
    ui->x_axis_samples->blockSignals(false);
}

void DataViewer_Widget::_updateGlobalScale(double scale) {
    ui->openGLWidget->setGlobalScale(static_cast<float>(scale));
}

void DataViewer_Widget::wheelEvent(QWheelEvent * event) {
    auto const numDegrees = static_cast<float>(event->angleDelta().y()) / 8.0f;
    auto const numSteps = numDegrees / 15.0f;
    
    auto const current_range = ui->x_axis_samples->value();
    
    float rangeFactor;
    if (_zoom_scaling_mode == ZoomScalingMode::Adaptive) {
        // Adaptive scaling: range factor is proportional to current range width
        // This makes adjustments more sensitive when zoomed in (small range), less sensitive when zoomed out (large range)
        rangeFactor = static_cast<float>(current_range) * 0.1f; // 10% of current range width
        
        // Clamp range factor to reasonable bounds
        rangeFactor = std::max(1.0f, std::min(rangeFactor, static_cast<float>(_time_frame->getTotalFrameCount()) / 100.0f));
    } else {
        // Fixed scaling (original behavior)
        rangeFactor = static_cast<float>(_time_frame->getTotalFrameCount()) / 10000.0f;
    }

    // Calculate range delta 
    // Wheel up (positive numSteps) should zoom IN (decrease range width)
    // Wheel down (negative numSteps) should zoom OUT (increase range width)
    auto const range_delta = static_cast<int64_t>(-numSteps * rangeFactor);
    
    // Apply range delta
    ui->openGLWidget->changeRangeWidth(range_delta);

    // Calculate new range width for spinbox update
    auto const new_range = std::max(1, current_range + static_cast<int>(range_delta));

    updateXAxisSamples(new_range);
    _updateLabels();
}

void DataViewer_Widget::_updateLabels() {
    auto x_axis = ui->openGLWidget->getXAxis();
    ui->neg_x_label->setText(QString::number(x_axis.getStart()));
    ui->pos_x_label->setText(QString::number(x_axis.getEnd()));
}

void DataViewer_Widget::_handleColorChanged(std::string const & feature_key, std::string const & hex_color) {
    // Update the color in the feature table
    ui->feature_table_widget->setFeatureColor(feature_key, hex_color);
    
    // Update the color in the OpenGL widget display options
    auto const type = _data_manager->getType(feature_key);
    
    if (type == DM_DataType::Analog) {
        auto config = ui->openGLWidget->getAnalogConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }
        
    } else if (type == DM_DataType::DigitalEvent) {
        auto config = ui->openGLWidget->getDigitalEventConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }
        
    } else if (type == DM_DataType::DigitalInterval) {
        auto config = ui->openGLWidget->getDigitalIntervalConfig(feature_key);
        if (config.has_value()) {
            config.value()->hex_color = hex_color;
        }
    }
    
    // Trigger a redraw
    ui->openGLWidget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
    
    std::cout << "Color changed for " << feature_key << " to " << hex_color << std::endl;
}

void DataViewer_Widget::_updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info) {
    // Convert time coordinate to actual time using the time frame
    int const time_index = static_cast<int>(std::round(time_coordinate));
    int const actual_time = _time_frame->getTimeAtIndex(time_index);
    
    QString coordinate_text;
    if (series_info.isEmpty()) {
        coordinate_text = QString("Coordinates: Time: %1 (index: %2), Canvas Y: %3")
                         .arg(actual_time)
                         .arg(time_index)
                         .arg(canvas_y, 0, 'f', 1);
    } else {
        coordinate_text = QString("Coordinates: Time: %1 (index: %2), %3")
                         .arg(actual_time)
                         .arg(time_index)
                         .arg(series_info);
    }
    
    ui->coordinate_label->setText(coordinate_text);
}

std::optional<AnalogTimeSeriesDisplayOptions *> DataViewer_Widget::getAnalogConfig(std::string const & key) const {
    return ui->openGLWidget->getAnalogConfig(key);
}

std::optional<DigitalEventSeriesDisplayOptions *> DataViewer_Widget::getDigitalEventConfig(std::string const & key) const {
    return ui->openGLWidget->getDigitalEventConfig(key);
}

std::optional<DigitalIntervalSeriesDisplayOptions *> DataViewer_Widget::getDigitalIntervalConfig(std::string const & key) const {
    return ui->openGLWidget->getDigitalIntervalConfig(key);
}
