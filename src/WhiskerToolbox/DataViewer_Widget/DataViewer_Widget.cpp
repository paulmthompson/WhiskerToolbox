#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"
#include "DataManager.hpp"
#include "DataViewerState.hpp"
#include "DataViewerStateData.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "Feature_Tree_Model.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "OpenGLWidget.hpp"
#include "SVGExporter.hpp"
#include "SpikeSorterConfigLoader.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"

#include "AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "DigitalEvent/EventViewer_Widget.hpp"
#include "DigitalInterval/IntervalViewer_Widget.hpp"

#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

DataViewer_Widget::DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     TimeScrollBar * time_scrollbar,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      _time_scrollbar{time_scrollbar},
      ui(new Ui::DataViewer_Widget),
      _state(std::make_shared<DataViewerState>()) {

    ui->setupUi(this);

    // Share state with OpenGLWidget
    ui->openGLWidget->setState(_state);

    // Note: Layout computation is now handled by OpenGLWidget's internal LayoutEngine
    // (Phase 4.9 migration - unified layout system)

    // Set up observer to automatically clean up data when it's deleted from DataManager
    // Queue the cleanup to the Qt event loop to avoid running during mid-update mutations
    _data_manager->addObserver([this]() {
        QPointer<DataViewer_Widget> self = this;
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->cleanupDeletedData(); }, Qt::QueuedConnection);
    });

    connect(time_scrollbar, &TimeScrollBar::timeChanged, this, &DataViewer_Widget::_updatePlot);

    // We should always get the master clock because we plot
    // Check for master clock
    auto time_keys = _data_manager->getTimeFrameKeys();
    // if timekeys doesn't have master, we should throw an error
    if (std::find(time_keys.begin(), time_keys.end(), TimeKey("master")) == time_keys.end()) {
        std::cout << "No master clock found in DataManager" << std::endl;
        _time_frame = _data_manager->getTime(TimeKey("time"));
    } else {
        _time_frame = _data_manager->getTime(TimeKey("master"));
    }

    // Set the master time frame for proper coordinate conversion
    ui->openGLWidget->setMasterTimeFrame(_time_frame);

    // Connect mouse hover signal from OpenGL widget
    connect(ui->openGLWidget, &OpenGLWidget::mouseHover,
            this, &DataViewer_Widget::_updateCoordinateDisplay);
}

DataViewer_Widget::~DataViewer_Widget() {
    delete ui;
}

void DataViewer_Widget::setState(std::shared_ptr<DataViewerState> state) {
    if (!state) {
        return;  // Don't accept null state
    }
    
    _state = std::move(state);
    
    // Share with OpenGLWidget
    ui->openGLWidget->setState(_state);
    
    // Connect viewStateChanged to update labels when time range changes
    connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
        _updateLabels();
    });
    
    _updateLabels();
}

void DataViewer_Widget::openWidget() {
    std::cout << "DataViewer Widget Opened" << std::endl;

    this->show();
    _updateLabels();
}

void DataViewer_Widget::closeEvent(QCloseEvent * event) {
    static_cast<void>(event);

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
    // Note: 'time' is a frame index from the scrollbar, for the "time" time frame

    if (_time_frame.get() != _data_manager->getTime(TimeKey("time")).get()) {
        auto time_in_ticks = _data_manager->getTime(TimeKey("time"))->getTimeAtIndex(TimeFrameIndex(time));
        TimeFrameIndex master_index = _time_frame->getIndexAtTime(static_cast<float>(time_in_ticks));
        ui->openGLWidget->updateCanvas(master_index);
    } else {
        ui->openGLWidget->updateCanvas(TimeFrameIndex(time));
    }

    _updateLabels();
}


void DataViewer_Widget::addFeature(std::string const & key, std::string const & color) {
    std::cout << "Adding feature: " << key << " with color: " << color << std::endl;
    _plotSelectedFeature(key, color);
}

void DataViewer_Widget::removeFeature(std::string const & key) {
    std::cout << "Removing feature: " << key << std::endl;
    _removeSelectedFeature(key);
}

void DataViewer_Widget::addFeatures(std::vector<std::string> const & keys, std::vector<std::string> const & colors) {
    std::cout << "Adding " << keys.size() << " features as group" << std::endl;

    // Mark batch add to suppress per-series auto-arrange
    _is_batch_add = true;
    // Process all features in the group without triggering individual canvas updates
    for (size_t i = 0; i < keys.size(); ++i) {
        std::string const & key = keys[i];
        std::string color = (i < colors.size()) ? colors[i] : "#FF6B6B";  // Default color if not provided
        _plotSelectedFeatureWithoutUpdate(key, color);
    }
    _is_batch_add = false;

    // Auto-arrange and auto-fill once after batch
    if (!keys.empty()) {
        std::cout << "Auto-arranging and filling canvas for group toggle" << std::endl;
        autoArrangeVerticalSpacing();// includes auto-fill functionality
    }

    // Trigger a single canvas update at the end
    if (!keys.empty()) {
        std::cout << "Triggering single canvas update for group toggle" << std::endl;
        ui->openGLWidget->updateCanvas();
    }
}

void DataViewer_Widget::removeFeatures(std::vector<std::string> const & keys) {
    std::cout << "Removing " << keys.size() << " features as group" << std::endl;

    _is_batch_add = true;
    // Process all features in the group without triggering individual canvas updates
    for (auto const & key: keys) {
        _removeSelectedFeatureWithoutUpdate(key);
    }
    _is_batch_add = false;

    // Auto-arrange and auto-fill once after batch
    if (!keys.empty()) {
        std::cout << "Auto-arranging and filling canvas for group toggle" << std::endl;
        autoArrangeVerticalSpacing();// includes auto-fill functionality
    }

    // Trigger a single canvas update at the end
    if (!keys.empty()) {
        std::cout << "Triggering single canvas update for group toggle" << std::endl;
        ui->openGLWidget->updateCanvas();
    }
}

void DataViewer_Widget::handleColorChanged(std::string const & feature_key, std::string const & hex_color) {
    // Update the color in state (not directly in OpenGL widget display options)
    auto const type = _data_manager->getType(feature_key);
    QString const qkey = QString::fromStdString(feature_key);

    if (type == DM_DataType::Analog) {
        auto * opts = _state->seriesOptions().getMutable<AnalogSeriesOptionsData>(qkey);
        if (opts) {
            opts->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::DigitalEvent) {
        auto * opts = _state->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(qkey);
        if (opts) {
            opts->hex_color() = hex_color;
        }
    } else if (type == DM_DataType::DigitalInterval) {
        auto * opts = _state->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(qkey);
        if (opts) {
            opts->hex_color() = hex_color;
        }
    }

    // Trigger a redraw
    ui->openGLWidget->updateCanvas();

    std::cout << "Color changed for " << feature_key << " to " << hex_color << std::endl;
}

void DataViewer_Widget::_plotSelectedFeature(std::string const & key, std::string const & color) {
    std::cout << "Attempting to plot feature: " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeature" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeature" << std::endl;
        return;
    }

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


        auto time_key = _data_manager->getTimeKey(key);
        std::cout << "Time frame key: " << time_key << std::endl;
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        std::cout << "Time frame has " << time_frame->getTotalFrameCount() << " frames" << std::endl;

        // Add series to OpenGL widget (LayoutEngine computes layout automatically)
        ui->openGLWidget->addAnalogTimeSeries(key, series, color);

        std::cout << "Successfully added analog series to OpenGL widget" << std::endl;

    } else if (data_type == DM_DataType::DigitalEvent) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalEventSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Add series to OpenGL widget (LayoutEngine computes layout automatically)
        ui->openGLWidget->addDigitalEventSeries(key, series, color);

    } else if (data_type == DM_DataType::DigitalInterval) {

        std::cout << "Adding << " << key << " to OpenGLWidget" << std::endl;
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalIntervalSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Add series to OpenGL widget (LayoutEngine computes layout automatically)
        ui->openGLWidget->addDigitalIntervalSeries(key, series, color);

    } else {
        std::cout << "Feature type not supported: " << convert_data_type_to_string(data_type) << std::endl;
        return;
    }

    // Auto-arrange and auto-fill canvas to make optimal use of space
    // IMPORTANT: Do not auto-arrange when adding DigitalInterval series, since intervals are
    // drawn full-canvas and should not affect analog/event stacking or global zoom.
    if (!_is_batch_add) {
        if (data_type == DM_DataType::DigitalInterval) {
            std::cout << "Skipping auto-arrange after adding DigitalInterval to preserve analog zoom" << std::endl;
        } else {
            std::cout << "Auto-arranging and filling canvas after adding series" << std::endl;
            autoArrangeVerticalSpacing();// This now includes auto-fill functionality
        }
    }

    std::cout << "Series addition and auto-arrangement completed" << std::endl;
    // Trigger canvas update to show the new series
    if (!_is_batch_add) {
        std::cout << "Triggering canvas update" << std::endl;
        ui->openGLWidget->updateCanvas();
    }
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

    // Remove from OpenGL widget (LayoutEngine recomputes layout automatically)
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
    // IMPORTANT: Do not auto-arrange when removing DigitalInterval series; removing an overlay
    // interval should not change analog/event stacking or global zoom.
    if (data_type == DM_DataType::DigitalInterval) {
        std::cout << "Skipping auto-arrange after removing DigitalInterval to preserve analog zoom" << std::endl;
    } else {
        std::cout << "Auto-arranging and filling canvas after removing series" << std::endl;
        autoArrangeVerticalSpacing();// This now includes auto-fill functionality
    }

    std::cout << "Series removal and auto-arrangement completed" << std::endl;
    // Trigger canvas update to reflect the removal
    std::cout << "Triggering canvas update after removal" << std::endl;
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::wheelEvent(QWheelEvent * event) {
    // Disable zooming while any interaction is active
    if (ui->openGLWidget->isInteractionActive()) {
        return;
    }

    auto const numDegrees = static_cast<float>(event->angleDelta().y()) / 8.0f;
    auto const numSteps = numDegrees / 15.0f;

    auto const & view_state = _state->viewState();
    auto const current_range = static_cast<int>(view_state.getTimeWidth());
    auto const total_frames = static_cast<float>(_time_frame->getTotalFrameCount());

    // Determine zoom sensitivity based on modifier keys:
    // - Shift: Fine mode (very small steps for precise adjustment)
    // - Ctrl: Coarse mode (large steps for quick navigation)
    // - None: Normal mode (balanced default behavior)
    bool const fine_mode = event->modifiers() & Qt::ShiftModifier;
    bool const coarse_mode = event->modifiers() & Qt::ControlModifier;

    float rangeFactor;
    if (_state->zoomScalingMode() == DataViewerZoomScalingMode::Adaptive) {
        // Adaptive scaling: range factor is proportional to current range width
        // Base percentages and clamps vary by mode
        float base_percentage;
        float min_clamp_divisor;
        float max_clamp_divisor;

        if (fine_mode) {
            // Fine mode: 1% of current range, very low minimum for precise control
            base_percentage = 0.01f;
            min_clamp_divisor = 10000.0f;  // 0.01% of total as minimum
            max_clamp_divisor = 100.0f;    // 1% of total as maximum
        } else if (coarse_mode) {
            // Coarse mode: 20% of current range for quick navigation
            base_percentage = 0.20f;
            min_clamp_divisor = 20.0f;     // 5% of total as minimum
            max_clamp_divisor = 5.0f;      // 20% of total as maximum
        } else {
            // Normal mode: 3% of current range (reduced from 10% for less sensitivity)
            base_percentage = 0.03f;
            min_clamp_divisor = 1000.0f;   // 0.1% of total as minimum
            max_clamp_divisor = 50.0f;     // 2% of total as maximum
        }

        rangeFactor = static_cast<float>(current_range) * base_percentage;

        // Clamp range factor to reasonable bounds
        float const min_range_factor = std::max(1.0f, total_frames / min_clamp_divisor);
        float const max_range_factor = total_frames / max_clamp_divisor;
        rangeFactor = std::max(min_range_factor, std::min(rangeFactor, max_range_factor));
    } else {
        // Fixed scaling mode with modifier support
        float divisor;
        if (fine_mode) {
            divisor = 100000.0f;  // Very fine steps
        } else if (coarse_mode) {
            divisor = 1000.0f;    // Coarse steps
        } else {
            divisor = 30000.0f;   // Normal steps (reduced from 10000 for less sensitivity)
        }
        rangeFactor = total_frames / divisor;
    }

    // Calculate range delta
    // Wheel up (positive numSteps) should zoom IN (decrease range width)
    // Wheel down (negative numSteps) should zoom OUT (increase range width)
    auto const range_delta = static_cast<int64_t>(-numSteps * rangeFactor);

    // Apply range delta directly to state
    _state->adjustTimeWidth(range_delta);

    _updateLabels();
}

void DataViewer_Widget::_updateLabels() {
    auto const & view_state = _state->viewState();
    ui->neg_x_label->setText(QString::number(view_state.time_start));
    ui->pos_x_label->setText(QString::number(view_state.time_end));
}

void DataViewer_Widget::_updateCoordinateDisplay(float time_coordinate, float canvas_y, QString const & series_info) {
    // Convert time coordinate to actual time using the time frame
    int const time_index = static_cast<int>(std::round(time_coordinate));
    int const actual_time = _time_frame->getTimeAtIndex(TimeFrameIndex(time_index));

    // Get canvas size for debugging
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();

    // Use fixed-width formatting to prevent label resizing
    // Reserve space for reasonable max values (time: 10 digits, index: 10 digits, Y: 8 chars, canvas: 5x5 digits)
    QString coordinate_text;
    if (series_info.isEmpty()) {
        coordinate_text = QString("Time: %1  Index: %2  Y: %3  Canvas: %4x%5")
                                  .arg(actual_time, 10)    // Right-aligned, width 10
                                  .arg(time_index, 10)     // Right-aligned, width 10
                                  .arg(canvas_y, 8, 'f', 1)// Right-aligned, width 8, 1 decimal
                                  .arg(canvas_width, 5)    // Right-aligned, width 5
                                  .arg(canvas_height, 5);  // Right-aligned, width 5
    } else {
        // For series info, still use fixed-width for numeric values but allow series info to vary
        coordinate_text = QString("Time: %1  Index: %2  %3  Canvas: %4x%5")
                                  .arg(actual_time, 10)
                                  .arg(time_index, 10)
                                  .arg(series_info, -30)// Left-aligned, min width 30
                                  .arg(canvas_width, 5)
                                  .arg(canvas_height, 5);
    }

    ui->coordinate_label->setText(coordinate_text);
}

// State access - mutable version (const version returns shared_ptr, defined in header)
DataViewerState * DataViewer_Widget::state() {
    return _state.get();
}

OpenGLWidget * DataViewer_Widget::getOpenGLWidget() const {
    return ui->openGLWidget;
}

void DataViewer_Widget::_plotSelectedFeatureWithoutUpdate(std::string const & key, std::string const & color) {
    std::cout << "Attempting to plot feature (batch): " << key << std::endl;

    if (key.empty()) {
        std::cerr << "Error: empty key in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    if (!_data_manager) {
        std::cerr << "Error: null data manager in _plotSelectedFeatureWithoutUpdate" << std::endl;
        return;
    }

    std::cout << "Using color: " << color << " for series: " << key << std::endl;

    auto data_type = _data_manager->getType(key);
    std::cout << "Feature type: " << convert_data_type_to_string(data_type) << std::endl;

    if (data_type == DM_DataType::Analog) {
        auto series = _data_manager->getData<AnalogTimeSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get AnalogTimeSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }

        // Add to OpenGL widget (which stores the series data)
        // Layout will be recomputed during render via computeAndApplyLayout()
        ui->openGLWidget->addAnalogTimeSeries(key, series, color);

    } else if (data_type == DM_DataType::DigitalEvent) {
        auto series = _data_manager->getData<DigitalEventSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalEventSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        // Add to OpenGL widget (which stores the series data)
        // Layout will be recomputed during render via computeAndApplyLayout()
        ui->openGLWidget->addDigitalEventSeries(key, series, color);

    } else if (data_type == DM_DataType::DigitalInterval) {
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        if (!series) {
            std::cerr << "Error: failed to get DigitalIntervalSeries for key: " << key << std::endl;
            return;
        }

        auto time_key = _data_manager->getTimeKey(key);
        auto time_frame = _data_manager->getTime(time_key);
        if (!time_frame) {
            std::cerr << "Error: failed to get TimeFrame for key: " << key << std::endl;
            return;
        }
        // Add to OpenGL widget (which stores the series data)
        // Layout will be recomputed during render via computeAndApplyLayout()
        ui->openGLWidget->addDigitalIntervalSeries(key, series, color);

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

    // Remove from OpenGLWidget - layout will be recomputed during next render
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
                // Check if this series is currently visible via state
                auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
                if (opts && opts->get_is_visible()) {
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
            float const std_dev = calculate_std_dev_approximate(*series);
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
        _state->setGlobalZoom(final_scale);

        std::cout << "Applied auto-scaling: vertical spacing = " << final_spacing
                  << ", global scale = " << final_scale << std::endl;

    } else {
        // If we can't calculate standard deviations, just apply spacing
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
            bool const in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
            if (!in_group) {
                // Check if this series is currently visible
                auto const * opts = _state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
                if (opts && opts->get_is_visible()) {
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

    // Calculate optimal event height (keep events clearly within their lane)
    // Use a conservative fraction of spacing so multiple stacked series remain visually distinct
    float const optimal_event_height = std::min(final_spacing * 0.3f, 0.2f);
    float const min_height = 0.01f;
    float const max_height = 0.5f;
    float const final_height = std::clamp(optimal_event_height, min_height, max_height);

    std::cout << "Calculated spacing: " << optimal_spacing << " pixels -> "
              << final_spacing << " normalized units" << std::endl;
    std::cout << "Calculated event height: " << final_height << " normalized units" << std::endl;

    // Apply the calculated settings to all event series in the group
    for (auto const & key: group_keys) {
        auto * opts = _state->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (opts) {
            opts->vertical_spacing = final_spacing;
            opts->event_height = final_height;
            opts->plotting_mode = EventPlottingModeData::Stacked;// Ensure stacked mode
        }
    }

    std::cout << "Applied auto-calculated event spacing: spacing = " << final_spacing
              << ", height = " << final_height << std::endl;
}

void DataViewer_Widget::autoArrangeVerticalSpacing() {
    std::cout << "DataViewer_Widget: Auto-arranging layout..." << std::endl;

    // Calculate and apply optimal scaling to fill the canvas
    _autoFillCanvas();

    // Trigger canvas update - layout will be recomputed via computeAndApplyLayout()
    ui->openGLWidget->updateCanvas();

    // Count total keys for logging
    auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
    auto const & event_map = ui->openGLWidget->getDigitalEventSeriesMap();
    auto const & interval_map = ui->openGLWidget->getDigitalIntervalSeriesMap();
    auto total_keys = analog_map.size() + event_map.size() + interval_map.size();

    std::cout << "DataViewer_Widget: Auto-arrange completed for " << total_keys << " series" << std::endl;
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

// ===== Context menu and configuration handling =====
void DataViewer_Widget::showGroupContextMenu(std::string const & group_name, QPoint const & global_pos) {
    QMenu menu;
    QMenu * loadMenu = menu.addMenu("Load configuration");
    QAction * loadSpikeSorter = loadMenu->addAction("spikesorter configuration");
    QAction * clearConfig = menu.addAction("Clear configuration");

    connect(loadSpikeSorter, &QAction::triggered, this, [this, group_name]() {
        _loadSpikeSorterConfigurationForGroup(QString::fromStdString(group_name));
    });
    connect(clearConfig, &QAction::triggered, this, [this, group_name]() {
        _clearConfigurationForGroup(QString::fromStdString(group_name));
    });

    menu.exec(global_pos);
}

void DataViewer_Widget::_loadSpikeSorterConfigurationForGroup(QString const & group_name) {
    // Open file dialog to select spike sorter configuration file
    QString path = QFileDialog::getOpenFileName(
            this,
            QString("Load spikesorter configuration for %1").arg(group_name),
            QString(),
            "Text Files (*.txt *.cfg *.conf);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QByteArray data = file.readAll();
    auto positions = _parseSpikeSorterConfig(data.toStdString());
    if (positions.empty()) return;

    // Load configuration into OpenGLWidget - layout will be recomputed on next render
    ui->openGLWidget->loadSpikeSorterConfiguration(group_name.toStdString(), positions);
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_clearConfigurationForGroup(QString const & group_name) {
    // Clear configuration in OpenGLWidget - layout will be recomputed on next render
    ui->openGLWidget->clearSpikeSorterConfiguration(group_name.toStdString());
    ui->openGLWidget->updateCanvas();
}

std::vector<ChannelPosition> DataViewer_Widget::_parseSpikeSorterConfig(std::string const & text) {
    return parseSpikeSorterConfig(text);
}

void DataViewer_Widget::_loadSpikeSorterConfigurationFromText(QString const & group_name, QString const & text) {
    auto positions = _parseSpikeSorterConfig(text.toStdString());
    if (positions.empty()) {
        std::cout << "No positions found in spike sorter configuration" << std::endl;
        return;
    }

    // Load configuration into OpenGLWidget - layout will be recomputed on next render
    ui->openGLWidget->loadSpikeSorterConfiguration(group_name.toStdString(), positions);
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_autoFillCanvas() {
    std::cout << "DataViewer_Widget: Auto-filling canvas..." << std::endl;

    // Get current canvas dimensions
    auto [canvas_width, canvas_height] = ui->openGLWidget->getCanvasSize();
    std::cout << "Canvas size: " << canvas_width << "x" << canvas_height << " pixels" << std::endl;

    // Count visible series from OpenGLWidget
    auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
    auto const & event_map = ui->openGLWidget->getDigitalEventSeriesMap();
    auto const & interval_map = ui->openGLWidget->getDigitalIntervalSeriesMap();

    // Collect visible keys
    std::vector<std::string> analog_keys;
    std::vector<std::string> event_keys;
    std::vector<std::string> interval_keys;

    for (auto const & [key, data]: analog_map) {
        auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            analog_keys.push_back(key);
        }
    }
    for (auto const & [key, data]: event_map) {
        auto const * opts = _state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            event_keys.push_back(key);
        }
    }
    for (auto const & [key, data]: interval_map) {
        auto const * opts = _state->seriesOptions().get<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
        if (opts && opts->get_is_visible()) {
            interval_keys.push_back(key);
        }
    }

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

    // Calculate and apply optimal event heights for digital event series
    if (visible_event_count > 0) {
        // Calculate event height conservatively to avoid near full-lane rendering
        // Use 30% of the spacing and cap at 0.2 to keep consistent scale across zooms
        float const optimal_event_height = std::min(final_spacing * 0.3f, 0.2f);

        std::cout << "Calculated optimal event height: " << optimal_event_height << " normalized units" << std::endl;

        // Apply optimal height to all visible digital event series
        for (auto const & key: event_keys) {
            auto * opts = _state->seriesOptions().getMutable<DigitalEventSeriesOptionsData>(QString::fromStdString(key));
            if (opts && opts->get_is_visible()) {
                opts->event_height = optimal_event_height;
                opts->plotting_mode = EventPlottingModeData::Stacked;// Ensure stacked mode
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
            auto * opts = _state->seriesOptions().getMutable<DigitalIntervalSeriesOptionsData>(QString::fromStdString(key));
            if (opts && opts->get_is_visible()) {
                opts->margin_factor = optimal_interval_height;
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

            auto const * opts = _state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(key));
            if (opts && opts->get_is_visible()) {
                auto series = _data_manager->getData<AnalogTimeSeries>(key);
                if (series) {
                    float std_dev = calculate_std_dev_approximate(*series);
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
            _state->setGlobalZoom(final_scale);
        }
    }

    std::cout << "Auto-fill canvas completed" << std::endl;
}

void DataViewer_Widget::cleanupDeletedData() {
    if (!_data_manager) {
        return;
    }

    // Collect keys that no longer exist in DataManager from OpenGLWidget's series maps
    std::vector<std::string> keys_to_cleanup;

    auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
    for (auto const & [key, data]: analog_map) {
        if (!_data_manager->getData<AnalogTimeSeries>(key)) {
            keys_to_cleanup.push_back(key);
        }
    }

    auto const & event_map = ui->openGLWidget->getDigitalEventSeriesMap();
    for (auto const & [key, data]: event_map) {
        if (!_data_manager->getData<DigitalEventSeries>(key)) {
            keys_to_cleanup.push_back(key);
        }
    }

    auto const & interval_map = ui->openGLWidget->getDigitalIntervalSeriesMap();
    for (auto const & [key, data]: interval_map) {
        if (!_data_manager->getData<DigitalIntervalSeries>(key)) {
            keys_to_cleanup.push_back(key);
        }
    }

    if (keys_to_cleanup.empty()) {
        return;
    }

    // De-duplicate keys in case the same key appears in multiple lists
    std::sort(keys_to_cleanup.begin(), keys_to_cleanup.end());
    keys_to_cleanup.erase(std::unique(keys_to_cleanup.begin(), keys_to_cleanup.end()), keys_to_cleanup.end());

    // Post cleanup to OpenGLWidget's thread safely
    QPointer<OpenGLWidget> glw = ui ? ui->openGLWidget : nullptr;
    if (glw) {
        QMetaObject::invokeMethod(glw, [glw, keys = keys_to_cleanup]() {
            if (!glw) return;
            for (auto const & key : keys) {
                glw->removeAnalogTimeSeries(key);
                glw->removeDigitalEventSeries(key);
                glw->removeDigitalIntervalSeries(key);
            } }, Qt::QueuedConnection);
    }

    // Re-arrange remaining data - layout will be recomputed via computeAndApplyLayout()
    autoArrangeVerticalSpacing();
}

void DataViewer_Widget::exportToSVG(bool includeScalebar, int scalebarLength) {
    std::cout << "SVG Export initiated" << std::endl;

    // Get save file path from user
    QString const fileName = QFileDialog::getSaveFileName(
            this,
            tr("Export Plot to SVG"),
            QString(),
            tr("SVG Files (*.svg);;All Files (*)"));

    if (fileName.isEmpty()) {
        std::cout << "SVG Export cancelled by user" << std::endl;
        return;
    }

    try {
        // Create SVG exporter with current plot state (OpenGLWidget has all needed state)
        SVGExporter exporter(ui->openGLWidget);

        // Configure scalebar if requested
        if (includeScalebar) {
            exporter.enableScalebar(true, scalebarLength);
        }

        // Generate SVG document
        QString const svg_content = exporter.exportToSVG();

        // Write to file
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(
                    this,
                    tr("Export Failed"),
                    tr("Could not open file for writing:\n%1").arg(fileName));
            std::cerr << "Failed to open file: " << fileName.toStdString() << std::endl;
            return;
        }

        QTextStream out(&file);
        out << svg_content;
        file.close();

        std::cout << "SVG Export successful: " << fileName.toStdString() << std::endl;

        // Show success message
        QMessageBox::information(
                this,
                tr("Export Successful"),
                tr("Plot exported to:\n%1\n\nCanvas size: %2x%3")
                        .arg(fileName)
                        .arg(exporter.getCanvasWidth())
                        .arg(exporter.getCanvasHeight()));

    } catch (std::exception const & e) {
        QMessageBox::critical(
                this,
                tr("Export Failed"),
                tr("An error occurred during export:\n%1").arg(e.what()));
        std::cerr << "SVG Export failed: " << e.what() << std::endl;
    }
}
