#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/AnalogTimeSeries/MVP_AnalogTimeSeries.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/MVP_DigitalEvent.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/MVP_DigitalInterval.hpp"
#include "DataViewer_Tree_Widget/DataViewer_Tree_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "DigitalEvent/EventViewer_Widget.hpp"
#include "DigitalInterval/IntervalViewer_Widget.hpp"

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

    // Initialize plotting manager with default viewport
    _plotting_manager = std::make_unique<PlottingManager>();

    // Provide PlottingManager reference to OpenGL widget
    ui->openGLWidget->setPlottingManager(_plotting_manager.get());

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

        // Collect all keys that belong to this group
        std::vector<std::string> group_keys;
        auto all_keys = _data_manager->getAllKeys();
        for (auto const & key: all_keys) {
            if (_data_manager->getType(key) == data_type) {
                // Check if this key belongs to the group by checking if it starts with the prefix
                if (key.find(group_prefix.toStdString()) == 0) {
                    group_keys.push_back(key);
                }
            }
        }

        std::cout << "Found " << group_keys.size() << " series in group " << group_prefix.toStdString() << std::endl;

        // Process all series in the group without triggering individual canvas updates
        for (auto const & key: group_keys) {
            if (enabled) {
                _plotSelectedFeatureWithoutUpdate(key);
            } else {
                _removeSelectedFeatureWithoutUpdate(key);
            }
        }

        // Auto-arrange and auto-fill when toggling a group to optimize space usage
        if (!group_keys.empty()) {
            std::cout << "Auto-arranging and filling canvas for group toggle" << std::endl;
            autoArrangeVerticalSpacing();// This now includes auto-fill functionality
        }

        // Trigger a single canvas update at the end
        if (!group_keys.empty()) {
            std::cout << "Triggering single canvas update for group toggle" << std::endl;
            ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());
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

    // Set the master time frame for proper coordinate conversion
    ui->openGLWidget->setMasterTimeFrame(_time_frame);

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

    // Grid line connections
    connect(ui->grid_lines_enabled, &QCheckBox::toggled, this, &DataViewer_Widget::_handleGridLinesToggled);
    connect(ui->grid_spacing, QOverload<int>::of(&QSpinBox::valueChanged), this, &DataViewer_Widget::_handleGridSpacingChanged);

    // Vertical spacing connection
    connect(ui->vertical_spacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &DataViewer_Widget::_handleVerticalSpacingChanged);

    // Auto-arrange button connection
    connect(ui->auto_arrange_button, &QPushButton::clicked, this, &DataViewer_Widget::autoArrangeVerticalSpacing);

    // Initialize grid line UI to match OpenGLWidget defaults
    ui->grid_lines_enabled->setChecked(ui->openGLWidget->getGridLinesEnabled());
    ui->grid_spacing->setValue(ui->openGLWidget->getGridSpacing());

    // Initialize vertical spacing UI to match OpenGLWidget defaults
    ui->vertical_spacing->setValue(static_cast<double>(ui->openGLWidget->getVerticalSpacing()));
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

    // Update plotting manager dimensions when widget is resized
    _updatePlottingManagerDimensions();

    // The OpenGL widget will automatically get its resizeGL called by Qt
    // but we can trigger an additional update if needed
    if (ui->openGLWidget) {
        ui->openGLWidget->update();
    }
}

void DataViewer_Widget::_updatePlot(int time) {
    //std::cout << "Time is " << time;
    time = _data_manager->getTime("time")->getTimeAtIndex(TimeIndex(time));
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

    // Register with plotting manager for coordinated positioning
    if (_plotting_manager) {
        std::cout << "Registering series with plotting manager: " << key << std::endl;
    }

    if (data_type == DM_DataType::Analog) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
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

        // Add to plotting manager first
        _plotting_manager->addAnalogSeries(key, series, time_frame, color);

        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);
        std::cout << "Successfully added analog series to PlottingManager and OpenGL widget" << std::endl;

    } else if (data_type == DM_DataType::DigitalEvent) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
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

        // Add to plotting manager first
        _plotting_manager->addDigitalEventSeries(key, series, time_frame, color);

        ui->openGLWidget->addDigitalEventSeries(key, series, time_frame, color);

    } else if (data_type == DM_DataType::DigitalInterval) {

        std::cout << "Adding << " << key << " to PlottingManager and OpenGLWidget" << std::endl;
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

        // Add to plotting manager first
        _plotting_manager->addDigitalIntervalSeries(key, series, time_frame, color);

        ui->openGLWidget->addDigitalIntervalSeries(key, series, time_frame, color);

    } else {
        std::cout << "Feature type not supported: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Apply coordinated plotting manager allocation after adding to OpenGL widget
    if (_plotting_manager) {
        _applyPlottingManagerAllocation(key);
    }

    // Auto-arrange and auto-fill canvas to make optimal use of space
    std::cout << "Auto-arranging and filling canvas after adding series" << std::endl;
    autoArrangeVerticalSpacing();// This now includes auto-fill functionality

    std::cout << "Series addition and auto-arrangement completed" << std::endl;
    // Trigger canvas update to show the new series
    std::cout << "Triggering canvas update" << std::endl;
    ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());
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

    // Remove from plotting manager first
    if (_plotting_manager) {
        bool removed = false;
        if (data_type == DM_DataType::Analog) {
            removed = _plotting_manager->removeAnalogSeries(key);
        } else if (data_type == DM_DataType::DigitalEvent) {
            removed = _plotting_manager->removeDigitalEventSeries(key);
        } else if (data_type == DM_DataType::DigitalInterval) {
            removed = _plotting_manager->removeDigitalIntervalSeries(key);
        }
        if (removed) {
            std::cout << "Unregistered '" << key << "' from plotting manager" << std::endl;
        }
    }

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

    // Auto-arrange and auto-fill canvas to rescale remaining elements
    std::cout << "Auto-arranging and filling canvas after removing series" << std::endl;
    autoArrangeVerticalSpacing();// This now includes auto-fill functionality

    std::cout << "Series removal and auto-arrangement completed" << std::endl;
    // Trigger canvas update to reflect the removal
    std::cout << "Triggering canvas update after removal" << std::endl;
    ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());
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
        int const stacked_widget_index = 1;// Analog widget is at index 1 (after empty page)
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto analog_widget = dynamic_cast<AnalogViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (analog_widget) {
            analog_widget->setActiveKey(key);
            std::cout << "Selected Analog Time Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to AnalogViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalInterval) {
        int const stacked_widget_index = 2;// Interval widget is at index 2
        ui->stackedWidget->setCurrentIndex(stacked_widget_index);
        auto interval_widget = dynamic_cast<IntervalViewer_Widget *>(ui->stackedWidget->widget(stacked_widget_index));
        if (interval_widget) {
            interval_widget->setActiveKey(key);
            std::cout << "Selected Digital Interval Series: " << key << std::endl;
        } else {
            std::cerr << "Error: failed to cast to IntervalViewer_Widget" << std::endl;
        }

    } else if (type == DM_DataType::DigitalEvent) {
        int const stacked_widget_index = 3;// Event widget is at index 3
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

    // Also update PlottingManager zoom factor
    if (_plotting_manager) {
        _plotting_manager->setGlobalZoom(static_cast<float>(scale));

        // Apply updated positions to all registered series
        auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
        for (auto const & key: analog_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
        for (auto const & key: event_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();
        for (auto const & key: interval_keys) {
            _applyPlottingManagerAllocation(key);
        }

        // Trigger canvas update
        ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());
    }
}

void DataViewer_Widget::wheelEvent(QWheelEvent * event) {
    // Disable zooming while dragging intervals
    if (ui->openGLWidget->isDraggingInterval()) {
        return;
    }

    auto const numDegrees = static_cast<float>(event->angleDelta().y()) / 8.0f;
    auto const numSteps = numDegrees / 15.0f;

    auto const current_range = ui->x_axis_samples->value();

    float rangeFactor;
    if (_zoom_scaling_mode == ZoomScalingMode::Adaptive) {
        // Adaptive scaling: range factor is proportional to current range width
        // This makes adjustments more sensitive when zoomed in (small range), less sensitive when zoomed out (large range)
        rangeFactor = static_cast<float>(current_range) * 0.1f;// 10% of current range width

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
    ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());

    std::cout << "Color changed for " << feature_key << " to " << hex_color << std::endl;
}

void DataViewer_Widget::_updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info) {
    // Convert time coordinate to actual time using the time frame
    int const time_index = static_cast<int>(std::round(time_coordinate));
    int const actual_time = _time_frame->getTimeAtIndex(TimeIndex(time_index));

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

std::optional<NewAnalogTimeSeriesDisplayOptions *> DataViewer_Widget::getAnalogConfig(std::string const & key) const {
    return ui->openGLWidget->getAnalogConfig(key);
}

std::optional<NewDigitalEventSeriesDisplayOptions *> DataViewer_Widget::getDigitalEventConfig(std::string const & key) const {
    return ui->openGLWidget->getDigitalEventConfig(key);
}

std::optional<NewDigitalIntervalSeriesDisplayOptions *> DataViewer_Widget::getDigitalIntervalConfig(std::string const & key) const {
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

void DataViewer_Widget::_handleGridLinesToggled(bool enabled) {
    ui->openGLWidget->setGridLinesEnabled(enabled);
}

void DataViewer_Widget::_handleGridSpacingChanged(int spacing) {
    ui->openGLWidget->setGridSpacing(spacing);
}

void DataViewer_Widget::_handleVerticalSpacingChanged(double spacing) {
    ui->openGLWidget->setVerticalSpacing(static_cast<float>(spacing));

    // Also update PlottingManager vertical scale
    if (_plotting_manager) {
        // Convert spacing to a scale factor relative to default (0.1f)
        float const scale_factor = static_cast<float>(spacing) / 0.1f;
        _plotting_manager->setGlobalVerticalScale(scale_factor);

        // Apply updated positions to all registered series
        auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
        for (auto const & key: analog_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
        for (auto const & key: event_keys) {
            _applyPlottingManagerAllocation(key);
        }
        auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();
        for (auto const & key: interval_keys) {
            _applyPlottingManagerAllocation(key);
        }

        // Trigger canvas update
        ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());
    }
}

void DataViewer_Widget::_plotSelectedFeatureWithoutUpdate(std::string const & key) {
    std::cout << "Attempting to plot feature (batch): " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    // Get color from tree widget
    std::string color = ui->feature_tree_widget->getSeriesColor(key);
    std::cout << "Using color: " << color << " for series: " << key << std::endl;

    auto data_type = _data_manager->getType(key);
    std::cout << "Feature type: " << convert_data_type_to_string(data_type) << std::endl;

    if (data_type == DM_DataType::Analog) {
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get AnalogTimeSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeFrame(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        ui->openGLWidget->addAnalogTimeSeries(key, series, time_frame, color);

    } else if (data_type == DM_DataType::DigitalEvent) {
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

    // Note: No canvas update triggered - this is for batch operations
    std::cout << "Successfully added series to OpenGL widget (batch mode)" << std::endl;
}

void DataViewer_Widget::_removeSelectedFeatureWithoutUpdate(std::string const & key) {
    std::cout << "Attempting to remove feature (batch): " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _removeSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _removeSelectedFeatureWithoutUpdate" << std::endl;
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

    // Note: No canvas update triggered - this is for batch operations
    std::cout << "Successfully removed series from OpenGL widget (batch mode)" << std::endl;
}

void DataViewer_Widget::_calculateOptimalScaling(std::vector<std::string> const & group_keys) {
    if (group_keys.empty()) {
        return;
    }

    std::cout << "Calculating optimal scaling for " << group_keys.size() << " analog channels..." << std::endl;

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count total number of currently visible analog series (including the new group)
    int total_visible_analog_series = static_cast<int>(group_keys.size());

    // Add any other already visible analog series
    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key: all_keys) {
        if (_data_manager->getType(key) == DM_DataType::Analog) {
            // Check if this key is already in our group (avoid double counting)
            bool in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
            if (!in_group) {
                // Check if this series is currently visible
                auto config = ui->openGLWidget->getAnalogConfig(key);
                if (config.has_value() && config.value()->is_visible) {
                    total_visible_analog_series++;
                }
            }
        }
    }

    std::cout << "Total visible analog series (including new group): " << total_visible_analog_series << std::endl;

    if (total_visible_analog_series <= 0) {
        return;// No series to scale
    }

    // Calculate optimal vertical spacing
    // Leave some margin at top and bottom (10% each = 20% total)
    float const effective_height = static_cast<float>(canvas_height) * 0.8f;
    float const optimal_spacing = effective_height / static_cast<float>(total_visible_analog_series);

    // Convert to normalized coordinates (OpenGL widget uses normalized spacing)
    // Assuming the widget's view height is typically around 2.0 units in normalized coordinates
    float const normalized_spacing = (optimal_spacing / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.01f;
    float const max_spacing = 1.0f;
    float const final_spacing = std::clamp(normalized_spacing, min_spacing, max_spacing);

    std::cout << "Calculated spacing: " << optimal_spacing << " pixels -> "
              << final_spacing << " normalized units" << std::endl;

    // Calculate optimal global gain based on standard deviations
    std::vector<float> std_devs;
    std_devs.reserve(group_keys.size());

    for (auto const & key: group_keys) {
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (series) {
            float const std_dev = calculate_std_dev(*series);
            std_devs.push_back(std_dev);
            std::cout << "Series " << key << " std dev: " << std_dev << std::endl;
        }
    }

    if (!std_devs.empty()) {
        // Use the median standard deviation as reference for scaling
        std::sort(std_devs.begin(), std_devs.end());
        float const median_std_dev = std_devs[std_devs.size() / 2];

        // Calculate optimal global scale
        // Target: each series should use about 60% of its allocated vertical space
        float const target_amplitude_fraction = 0.6f;
        float const target_amplitude_in_pixels = optimal_spacing * target_amplitude_fraction;

        // Convert to normalized coordinates (3 standard deviations should fit in target amplitude)
        float const target_amplitude_normalized = (target_amplitude_in_pixels / static_cast<float>(canvas_height)) * 2.0f;
        float const three_sigma_target = target_amplitude_normalized;

        // Calculate scale factor needed
        float const optimal_global_scale = three_sigma_target / (3.0f * median_std_dev);

        // Clamp to reasonable bounds
        float const min_scale = 0.1f;
        float const max_scale = 100.0f;
        float const final_scale = std::clamp(optimal_global_scale, min_scale, max_scale);

        std::cout << "Median std dev: " << median_std_dev
                  << ", target amplitude: " << target_amplitude_in_pixels << " pixels"
                  << ", optimal global scale: " << final_scale << std::endl;

        // Apply the calculated settings
        ui->vertical_spacing->setValue(static_cast<double>(final_spacing));
        ui->global_zoom->setValue(static_cast<double>(final_scale));

        std::cout << "Applied auto-scaling: vertical spacing = " << final_spacing
                  << ", global scale = " << final_scale << std::endl;

    } else {
        // If we can't calculate standard deviations, just apply spacing
        ui->vertical_spacing->setValue(static_cast<double>(final_spacing));
        std::cout << "Applied auto-spacing only: vertical spacing = " << final_spacing << std::endl;
    }
}

void DataViewer_Widget::_calculateOptimalEventSpacing(std::vector<std::string> const & group_keys) {
    if (group_keys.empty()) {
        return;
    }

    std::cout << "Calculating optimal event spacing for " << group_keys.size() << " digital event series..." << std::endl;

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count total number of currently visible digital event series (including the new group)
    int total_visible_event_series = static_cast<int>(group_keys.size());

    // Add any other already visible digital event series
    auto all_keys = _data_manager->getAllKeys();
    for (auto const & key: all_keys) {
        if (_data_manager->getType(key) == DM_DataType::DigitalEvent) {
            // Check if this key is already in our group (avoid double counting)
            bool in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
            if (!in_group) {
                // Check if this series is currently visible
                auto config = ui->openGLWidget->getDigitalEventConfig(key);
                if (config.has_value() && config.value()->is_visible) {
                    total_visible_event_series++;
                }
            }
        }
    }

    std::cout << "Total visible digital event series (including new group): " << total_visible_event_series << std::endl;

    if (total_visible_event_series <= 0) {
        return;// No series to scale
    }

    // Calculate optimal vertical spacing
    // Leave some margin at top and bottom (10% each = 20% total)
    float const effective_height = static_cast<float>(canvas_height) * 0.8f;
    float const optimal_spacing = effective_height / static_cast<float>(total_visible_event_series);

    // Convert to normalized coordinates (OpenGL widget uses normalized spacing)
    // Assuming the widget's view height is typically around 2.0 units in normalized coordinates
    float const normalized_spacing = (optimal_spacing / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.01f;
    float const max_spacing = 1.0f;
    float const final_spacing = std::clamp(normalized_spacing, min_spacing, max_spacing);

    // Calculate optimal event height (should be smaller than spacing to avoid overlap)
    float const optimal_event_height = final_spacing * 0.6f;// 60% of spacing for visible separation
    float const min_height = 0.01f;
    float const max_height = 0.5f;
    float const final_height = std::clamp(optimal_event_height, min_height, max_height);

    std::cout << "Calculated spacing: " << optimal_spacing << " pixels -> "
              << final_spacing << " normalized units" << std::endl;
    std::cout << "Calculated event height: " << final_height << " normalized units" << std::endl;

    // Apply the calculated settings to all event series in the group
    for (auto const & key: group_keys) {
        auto config = ui->openGLWidget->getDigitalEventConfig(key);
        if (config.has_value()) {
            config.value()->vertical_spacing = final_spacing;
            config.value()->event_height = final_height;
            config.value()->display_mode = EventDisplayMode::Stacked;// Ensure stacked mode
        }
    }

    std::cout << "Applied auto-calculated event spacing: spacing = " << final_spacing
              << ", height = " << final_height << std::endl;
}

void DataViewer_Widget::autoArrangeVerticalSpacing() {
    std::cout << "DataViewer_Widget: Auto-arranging with plotting manager..." << std::endl;

    // Update dimensions first
    _updatePlottingManagerDimensions();

    // Apply new allocations to all registered series
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
    auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();

    for (auto const & key: analog_keys) {
        _applyPlottingManagerAllocation(key);
    }
    for (auto const & key: event_keys) {
        _applyPlottingManagerAllocation(key);
    }
    for (auto const & key: interval_keys) {
        _applyPlottingManagerAllocation(key);
    }

    // Calculate and apply optimal scaling to fill the canvas
    _autoFillCanvas();

    // Update OpenGL widget view bounds based on content height
    _updateViewBounds();

    // Trigger canvas update to show new positions
    ui->openGLWidget->updateCanvas(_data_manager->getCurrentTime());

    int total_keys = analog_keys.size() + event_keys.size() + interval_keys.size();
    std::cout << "DataViewer_Widget: Auto-arrange completed for " << total_keys << " series" << std::endl;
}

void DataViewer_Widget::_updateViewBounds() {
    if (!_plotting_manager) {
        return;
    }

    // PlottingManager uses normalized coordinates, so view bounds are typically -1 to +1
    // For now, use standard bounds but this enables future enhancement
    std::cout << "DataViewer_Widget: Using standard view bounds with PlottingManager" << std::endl;
}

std::string DataViewer_Widget::_convertDataType(DM_DataType dm_type) const {
    switch (dm_type) {
        case DM_DataType::Analog:
            return "Analog";
        case DM_DataType::DigitalEvent:
            return "DigitalEvent";
        case DM_DataType::DigitalInterval:
            return "DigitalInterval";
        default:
            // For unsupported types, default to Analog
            // This should be rare in practice given our type filters
            std::cerr << "Warning: Unsupported data type " << convert_data_type_to_string(dm_type)
                      << " defaulting to Analog for plotting manager" << std::endl;
            return "Analog";
    }
}

void DataViewer_Widget::_updatePlottingManagerDimensions() {
    if (!_plotting_manager) {
        return;
    }

    // Get current canvas dimensions from OpenGL widget
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();

    // PlottingManager works in normalized device coordinates, so no specific dimension update needed
    // But we could update viewport bounds if needed in the future

    std::cout << "DataViewer_Widget: Updated plotting manager dimensions: "
              << canvas_width << "x" << canvas_height << " pixels" << std::endl;
}

void DataViewer_Widget::_applyPlottingManagerAllocation(std::string const & series_key) {
    if (!_plotting_manager) {
        return;
    }

    auto data_type = _data_manager->getType(series_key);

    std::cout << "DataViewer_Widget: Applying plotting manager allocation for '" << series_key << "'" << std::endl;

    // For now, use a basic implementation that will be enhanced when we update OpenGLWidget
    // The main goal is to get the compilation working first

    // Apply positioning based on data type
    if (data_type == DM_DataType::Analog) {
        auto config = ui->openGLWidget->getAnalogConfig(series_key);
        if (config.has_value()) {
            // Basic allocation - will be properly implemented when OpenGL widget is updated
            std::cout << "  Applied basic allocation to analog '" << series_key << "'" << std::endl;
        }

    } else if (data_type == DM_DataType::DigitalEvent) {
        auto config = ui->openGLWidget->getDigitalEventConfig(series_key);
        if (config.has_value()) {
            // Basic allocation - will be properly implemented when OpenGL widget is updated
            std::cout << "  Applied basic allocation to event '" << series_key << "'" << std::endl;
        }

    } else if (data_type == DM_DataType::DigitalInterval) {
        auto config = ui->openGLWidget->getDigitalIntervalConfig(series_key);
        if (config.has_value()) {
            // Basic allocation - will be properly implemented when OpenGL widget is updated
            std::cout << "  Applied basic allocation to interval '" << series_key << "'" << std::endl;
        }
    }
}

void DataViewer_Widget::_autoFillCanvas() {
    std::cout << "DataViewer_Widget: Auto-filling canvas with PlottingManager..." << std::endl;

    if (!_plotting_manager) {
        std::cout << "No plotting manager available" << std::endl;
        return;
    }

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count visible series using PlottingManager
    auto analog_keys = _plotting_manager->getVisibleAnalogSeriesKeys();
    auto event_keys = _plotting_manager->getVisibleDigitalEventSeriesKeys();
    auto interval_keys = _plotting_manager->getVisibleDigitalIntervalSeriesKeys();

    int visible_analog_count = static_cast<int>(analog_keys.size());
    int visible_event_count = static_cast<int>(event_keys.size());
    int visible_interval_count = static_cast<int>(interval_keys.size());

    int total_visible = visible_analog_count + visible_event_count + visible_interval_count;
    std::cout << "Visible series: " << visible_analog_count << " analog, "
              << visible_event_count << " events, " << visible_interval_count
              << " intervals (total: " << total_visible << ")" << std::endl;

    if (total_visible == 0) {
        std::cout << "No visible series to auto-scale" << std::endl;
        return;
    }

    // Calculate optimal vertical spacing to fill canvas
    // Use 90% of canvas height, leaving 5% margin at top and bottom
    float const usable_height = static_cast<float>(canvas_height) * 0.9f;
    float const optimal_spacing_pixels = usable_height / static_cast<float>(total_visible);

    // Convert to normalized coordinates (assuming 2.0 total normalized height)
    float const optimal_spacing_normalized = (optimal_spacing_pixels / static_cast<float>(canvas_height)) * 2.0f;

    // Clamp to reasonable bounds
    float const min_spacing = 0.02f;
    float const max_spacing = 1.5f;
    float const final_spacing = std::clamp(optimal_spacing_normalized, min_spacing, max_spacing);

    std::cout << "Calculated optimal spacing: " << optimal_spacing_pixels << " pixels -> "
              << final_spacing << " normalized units" << std::endl;

    // Apply the calculated vertical spacing
    ui->vertical_spacing->setValue(static_cast<double>(final_spacing));

    // Calculate and apply optimal event heights for digital event series
    if (visible_event_count > 0) {
        // Calculate optimal event height to fill most of the allocated space
        // Use 80% of the spacing to leave some visual separation between events
        float const optimal_event_height = final_spacing * 0.8f;

        std::cout << "Calculated optimal event height: " << optimal_event_height << " normalized units" << std::endl;

        // Apply optimal height to all visible digital event series
        for (auto const & key: event_keys) {
            auto config = ui->openGLWidget->getDigitalEventConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                config.value()->event_height = optimal_event_height;
                config.value()->display_mode = EventDisplayMode::Stacked;// Ensure stacked mode
                std::cout << "  Applied event height " << optimal_event_height
                          << " to series '" << key << "'" << std::endl;
            }
        }
    }

    // Calculate and apply optimal interval heights for digital interval series
    if (visible_interval_count > 0) {
        // Calculate optimal interval height to fill most of the allocated space
        // Use 80% of the spacing to leave some visual separation between intervals
        float const optimal_interval_height = final_spacing * 0.8f;

        std::cout << "Calculated optimal interval height: " << optimal_interval_height << " normalized units" << std::endl;

        // Apply optimal height to all visible digital interval series
        for (auto const & key: interval_keys) {
            auto config = ui->openGLWidget->getDigitalIntervalConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                config.value()->interval_height = optimal_interval_height;
                std::cout << "  Applied interval height " << optimal_interval_height
                          << " to series '" << key << "'" << std::endl;
            }
        }
    }

    // Calculate optimal global scale for analog series to use their allocated space effectively
    if (visible_analog_count > 0) {
        // Sample a few analog series to estimate appropriate scaling
        std::vector<float> sample_std_devs;
        sample_std_devs.reserve(std::min(5, visible_analog_count));// Sample up to 5 series

        int sampled = 0;
        for (auto const & key: analog_keys) {
            if (sampled >= 5) break;

            auto config = ui->openGLWidget->getAnalogConfig(key);
            if (config.has_value() && config.value()->is_visible) {
                auto series = _data_manager->getData<AnalogTimeSeries>(key);
                if (series) {
                    float std_dev = calculate_std_dev(*series);
                    if (std_dev > 0.0f) {
                        sample_std_devs.push_back(std_dev);
                        sampled++;
                    }
                }
            }
        }

        if (!sample_std_devs.empty()) {
            // Use median standard deviation for scaling calculation
            std::sort(sample_std_devs.begin(), sample_std_devs.end());
            float median_std_dev = sample_std_devs[sample_std_devs.size() / 2];

            // Calculate scale so that ±3 standard deviations use ~60% of allocated space
            float const target_amplitude_fraction = 0.6f;
            float const target_amplitude_pixels = optimal_spacing_pixels * target_amplitude_fraction;
            float const target_amplitude_normalized = (target_amplitude_pixels / static_cast<float>(canvas_height)) * 2.0f;

            // For ±3σ coverage
            float const three_sigma_coverage = target_amplitude_normalized;
            float const optimal_global_scale = three_sigma_coverage / (6.0f * median_std_dev);

            // Clamp to reasonable bounds
            float const min_scale = 0.01f;
            float const max_scale = 100.0f;
            float const final_scale = std::clamp(optimal_global_scale, min_scale, max_scale);

            std::cout << "Calculated optimal global scale: median_std_dev=" << median_std_dev
                      << ", target_amplitude=" << target_amplitude_pixels << " pixels"
                      << ", final_scale=" << final_scale << std::endl;

            // Apply the calculated global scale
            ui->global_zoom->setValue(static_cast<double>(final_scale));
        }
    }

    std::cout << "Auto-fill canvas completed" << std::endl;
}
