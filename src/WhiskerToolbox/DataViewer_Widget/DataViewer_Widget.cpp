#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "DataManager.hpp"

#include "DataViewer_Tree_Widget/DataViewer_Tree_Widget.hpp"
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

    ui->feature_tree_widget->setTypeFilter({DM_DataType::Analog, DM_DataType::DigitalEvent, DM_DataType::DigitalInterval});

    ui->feature_tree_widget->setDataManager(_data_manager);

    connect(ui->feature_tree_widget, &DataViewer_Tree_Widget::seriesSelected, this, &DataViewer_Widget::_handleFeatureSelected);
    connect(ui->feature_tree_widget, &DataViewer_Tree_Widget::seriesToggled, this, [this](QString const & series_key, bool enabled) {
        std::cout << "Signal received in DataViewer_Widget: " << series_key.toStdString() << " enabled: " << enabled << std::endl;
        _addFeatureToModel(series_key, enabled);
    });

    connect(ui->feature_tree_widget, &DataViewer_Tree_Widget::groupToggled, this, [this](QString const & group_prefix, DM_DataType data_type, bool enabled) {
        // Handle group toggle - enable/disable all series in the group
        std::cout << "Group " << group_prefix.toStdString() << " toggled: " << enabled << std::endl;
        
        // Get all keys of the specified data type
        auto all_keys = _data_manager->getAllKeys();
        for (auto const & key : all_keys) {
            if (_data_manager->getType(key) == data_type) {
                // Check if this key belongs to the group by checking if it starts with the prefix
                if (key.find(group_prefix.toStdString()) == 0) {
                    // This series belongs to the group, toggle it
                    if (enabled) {
                        _plotSelectedFeature(key);
                    } else {
                        _removeSelectedFeature(key);
                    }
                }
            }
        }
    });

    connect(ui->x_axis_samples, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleXAxisSamplesChanged);

    connect(ui->global_zoom, &QDoubleSpinBox::valueChanged, this, &DataViewer_Widget::_updateGlobalScale);

    connect(ui->theme_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DataViewer_Widget::_handleThemeChanged);

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
    
    // Tree is already populated by observer pattern in setDataManager()
    // Only populate if tree is somehow empty (fallback safety check)
    if (ui->feature_tree_widget->topLevelItemCount() == 0) {
        std::cout << "Tree is empty, triggering manual population as fallback" << std::endl;
        ui->feature_tree_widget->populateTree();
    }
    
    this->show();

    _updateLabels();
}

void DataViewer_Widget::closeEvent(QCloseEvent * event) {
    std::cout << "Close event detected" << std::endl;
}

void DataViewer_Widget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    
    // The OpenGL widget will automatically get its resizeGL called by Qt
    // but we can trigger an additional update if needed
    if (ui->openGLWidget) {
        ui->openGLWidget->update();
    }
}

void DataViewer_Widget::_updatePlot(int time) {
    //std::cout << "Time is " << time;
    time = _data_manager->getTime("time")->getTimeAtIndex(time);
    //std::cout << ""
    ui->openGLWidget->updateCanvas(time);

    _updateLabels();
}


void DataViewer_Widget::_addFeatureToModel(QString const & feature, bool enabled) {
    std::cout << "Feature toggle signal received: " << feature.toStdString() << " enabled: " << enabled << std::endl;

    if (enabled) {
        _plotSelectedFeature(feature.toStdString());
    } else {
        _removeSelectedFeature(feature.toStdString());
    }
}

void DataViewer_Widget::_plotSelectedFeature(std::string const & key) {
    std::cout << "Attempting to plot feature: " << key << std::endl;
    
    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeature" << std::endl;
        return;
    }
    
    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeature" << std::endl;
        return;
    }

    // Get color from tree widget
    std::string color = ui->feature_tree_widget->getSeriesColor(key);
    std::cout << "Using color: " << color << " for series: " << key << std::endl;

    auto data_type = _data_manager->getType(key);
    std::cout << "Feature type: " << convert_data_type_to_string(data_type) << std::endl;

    if (data_type == DM_DataType::Analog) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get AnalogTimeSeries for key: " << key << std::endl;
            return;
        }

        
        auto time_key = _data_manager->getTimeFrame(key);
        std::cout << "Time frame key: " << time_key << std::endl;
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        
        std::cout << "Time frame has " << time_frame->getTotalFrameCount() << " frames" << std::endl;
        
        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);
        std::cout << "Successfully added analog series to OpenGL widget" << std::endl;

    } else if (data_type == DM_DataType::DigitalEvent) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalEventSeries for key: " << key << std::endl;
            return;
        }
                
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        
        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);
        
    } else if (data_type == DM_DataType::DigitalInterval) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalIntervalSeries for key: " << key << std::endl;
            return;
        }
                
        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        
        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);
        
    } else {
        std::cout << "Feature type not supported: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }
    
    // Trigger canvas update to show the new series
    std::cout << "Triggering canvas update" << std::endl;
    ui->openGLWidget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
    std::cout << "Canvas update completed" << std::endl;
}

void DataViewer_Widget::_removeSelectedFeature(std::string const & key) {
    std::cout << "Attempting to remove feature: " << key << std::endl;
    
    if (key.empty()) {
        std::cerr << "Error: empty key in _removeSelectedFeature" << std::endl;
        return;
    }
    
    if (!_data_manager) {
        std::cerr << "Error: null data manager in _removeSelectedFeature" << std::endl;
        return;
    }
    
    auto data_type = _data_manager->getType(key);
    
    if (data_type == DM_DataType::Analog) {
        ui->openGLWidget->removeAnalogTimeSeries(key);
    } else if (data_type == DM_DataType::DigitalEvent) {
        ui->openGLWidget->removeDigitalEventSeries(key);
    } else if (data_type == DM_DataType::DigitalInterval) {
        ui->openGLWidget->removeDigitalIntervalSeries(key);
    } else {
        std::cout << "Feature type not supported for removal: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }
    
    // Trigger canvas update to reflect the removal
    std::cout << "Triggering canvas update after removal" << std::endl;
    ui->openGLWidget->updateCanvas(_data_manager->getTime()->getLastLoadedFrame());
}

void DataViewer_Widget::_handleFeatureSelected(QString const & feature) {
    std::cout << "Feature selected signal received: " << feature.toStdString() << std::endl;
    
    if (feature.isEmpty()) {
        std::cerr << "Error: empty feature name in _handleFeatureSelected" << std::endl;
        return;
    }
    
    if (!_data_manager) {
        std::cerr << "Error: null data manager in _handleFeatureSelected" << std::endl;
        return;
    }
    
    _highlighted_available_feature = feature;
    
    // Switch stacked widget based on data type
    auto const type = _data_manager->getType(feature.toStdString());
    auto key = feature.toStdString();
    
    std::cout << "Feature type for selection: " << convert_data_type_to_string(type) << std::endl;

    if (type == DM_DataType::Analog) {
        int const stacked_widget_index = 1; // Analog widget is at index 1 (after empty page)
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto analog_widget = dynamic_cast<AnalogViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (analog_widget) {
            analog_widget->setActiveKey(key);
            std::cout << "Selected Analog Time Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to AnalogViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 2; // Interval widget is at index 2
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<IntervalViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (interval_widget) {
            interval_widget->setActiveKey(key);
            std::cout << "Selected Digital Interval Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to IntervalViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalEvent) {
        int const stacked_widget_index = 3; // Event widget is at index 3
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto event_widget = dynamic_cast<EventViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (event_widget) {
            event_widget->setActiveKey(key);
            std::cout << "Selected Digital Event Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to EventViewer_Widget" << std::endl;
        }

    } else {
        // No specific widget for this type, don't change the current index
        std::cout << "Unsupported feature type for detailed view: " << convert_data_type_to_string(type) << std::endl;
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
    // Update the color in the OpenGL widget display options (tree widget color management will be added later)
    
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
    
    // Get canvas size for debugging
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    
    QString coordinate_text;
    if (series_info.isEmpty()) {
        coordinate_text = QString("Coordinates: Time: %1 (index: %2), Canvas Y: %3 | Canvas: %4x%5")
                         .arg(actual_time)
                         .arg(time_index)
                         .arg(canvas_y, 0, 'f', 1)
                         .arg(canvas_width)
                         .arg(canvas_height);
    } else {
        coordinate_text = QString("Coordinates: Time: %1 (index: %2), %3 | Canvas: %4x%5")
                         .arg(actual_time)
                         .arg(time_index)
                         .arg(series_info)
                         .arg(canvas_width)
                         .arg(canvas_height);
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

void DataViewer_Widget::_handleThemeChanged(int theme_index) {
    PlotTheme theme = (theme_index == 0) ? PlotTheme::Dark : PlotTheme::Light;
    ui->openGLWidget->setPlotTheme(theme);
    
    // Update coordinate label styling based on theme
    if (theme == PlotTheme::Dark) {
        ui->coordinate_label->setStyleSheet("background-color: rgba(0, 0, 0, 50); color: white; padding: 2px;");
    } else {
        ui->coordinate_label->setStyleSheet("background-color: rgba(255, 255, 255, 50); color: black; padding: 2px;");
    }
    
    std::cout << "Theme changed to: " << (theme_index == 0 ? "Dark" : "Light") << std::endl;
}
