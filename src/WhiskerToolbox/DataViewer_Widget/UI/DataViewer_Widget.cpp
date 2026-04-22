#include "DataViewer_Widget.hpp"
#include "ui_DataViewer_Widget.h"

#include "Core/DataViewerState.hpp"
#include "Core/DataViewerStateData.hpp"
#include "CorePlotting/CoordinateTransform/ViewStateData.hpp"
#include "Feature_Tree_Model.hpp"
#include "Ordering/SpikeToAnalogPairingLoader.hpp"
#include "Ordering/SwindaleSpikeSorterLoader.hpp"
#include "Rendering/OpenGLWidget.hpp"
#include "Rendering/SVGExporter.hpp"
#include "SubWidgets/AnalogTimeSeries/AnalogViewer_Widget.hpp"
#include "SubWidgets/DigitalEvent/EventViewer_Widget.hpp"
#include "SubWidgets/DigitalInterval/IntervalViewer_Widget.hpp"
#include "UI/SpikeToAnalogConfigDialog.hpp"

#include "Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisState.hpp"
#include "Plots/Common/MultiLaneVerticalAxisWidget/Core/MultiLaneVerticalAxisStateData.hpp"
#include "Plots/Common/MultiLaneVerticalAxisWidget/MultiLaneVerticalAxisWidget.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/utils/statistics.hpp"
#include "DataManager.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogSeriesHelpers.hpp"
#include "DataViewer/AnalogTimeSeries/AnalogTimeSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalEvent/DigitalEventSeriesDisplayOptions.hpp"
#include "DataViewer/DigitalInterval/DigitalIntervalSeriesDisplayOptions.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QFile>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QTableWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QWheelEvent>

#include "StateManagement/AppFileDialog.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_map>

namespace {

/**
 * @brief Extract group name from a series key
 * 
 * Strips a trailing _N suffix (where N is one or more digits) to derive
 * the group name. For example, "voltage_3" returns "voltage".
 * If no numeric suffix is found, returns the key unchanged.
 */
std::string extractGroupName(std::string const & key) {
    auto const pos = key.rfind('_');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= key.size()) {
        return key;
    }
    bool all_digits = true;
    for (size_t i = pos + 1; i < key.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(key[i])) == 0) {
            all_digits = false;
            break;
        }
    }
    if (all_digits) {
        return key.substr(0, pos);
    }
    return key;
}

[[nodiscard]] std::string laneIdForSeries(DataViewerState const * state,
                                          std::string const & series_id) {
    if (state != nullptr) {
        if (auto const * override_data = state->getSeriesLaneOverride(series_id);
            override_data != nullptr && !override_data->lane_id.empty()) {
            return override_data->lane_id;
        }
    }
    return std::string("__auto_lane__") + series_id;
}

[[nodiscard]] bool isStackableLaneSeries(DataViewerState const * state,
                                         DataManager const * data_manager,
                                         std::string const & series_id) {
    if (state == nullptr || data_manager == nullptr) {
        return false;
    }

    DM_DataType const type = data_manager->getType(series_id);
    if (type == DM_DataType::Analog) {
        auto const * opts = state->seriesOptions().get<AnalogSeriesOptionsData>(QString::fromStdString(series_id));
        return opts != nullptr && opts->get_is_visible();
    }

    if (type == DM_DataType::DigitalEvent) {
        auto const * opts = state->seriesOptions().get<DigitalEventSeriesOptionsData>(QString::fromStdString(series_id));
        return opts != nullptr && opts->get_is_visible() && opts->plotting_mode == EventPlottingModeData::Stacked;
    }

    return false;
}

struct LaneAggregate {
    std::string lane_id;
    std::vector<std::string> series_ids;
    float y_center{0.0f};
    float y_extent{0.0f};
    int first_series_index{std::numeric_limits<int>::max()};
};

[[nodiscard]] std::string chooseLaneLabel(DataViewerState const * state,
                                          DataManager const * data_manager,
                                          LaneAggregate const & lane) {
    if (state != nullptr) {
        if (auto const * lane_override = state->getLaneOverride(lane.lane_id);
            lane_override != nullptr && !lane_override->display_label.empty()) {
            return lane_override->display_label;
        }
    }

    if (lane.series_ids.empty()) {
        return lane.lane_id;
    }

    if (lane.series_ids.size() == 1) {
        return lane.series_ids.front();
    }

    std::string primary_label = lane.series_ids.front();
    if (data_manager != nullptr) {
        auto analog_it = std::find_if(lane.series_ids.begin(), lane.series_ids.end(),
                                      [data_manager](std::string const & key) {
                                          return data_manager->getType(key) == DM_DataType::Analog;
                                      });
        if (analog_it != lane.series_ids.end()) {
            primary_label = *analog_it;
        }
    }

    return primary_label + " +" + std::to_string(lane.series_ids.size() - 1);
}

}// anonymous namespace

DataViewer_Widget::DataViewer_Widget(std::shared_ptr<DataManager> data_manager,
                                     QWidget * parent)
    : QWidget(parent),
      _data_manager{std::move(data_manager)},
      ui(new Ui::DataViewer_Widget),
      _state(std::make_shared<DataViewerState>()) {

    ui->setupUi(this);

    // Share state with OpenGLWidget
    ui->openGLWidget->setState(_state);

    // --- Multi-Lane Vertical Axis Widget ---
    // Insert a QHBoxLayout wrapping the axis widget + OpenGLWidget.
    // The .ui file places openGLWidget directly in plot_layout (QVBoxLayout).
    // We remove it, wrap it in an HBox with the axis widget, and re-insert.
    {
        auto * axis_state = _state->multiLaneAxisState();
        _multi_lane_axis_widget = new MultiLaneVerticalAxisWidget(axis_state, this);

        // Viewport getter: returns effective Y bounds accounting for zoom and pan
        _multi_lane_axis_widget->setViewportGetter([this]() -> std::pair<float, float> {
            auto const eff = CorePlotting::computeEffectiveYViewport(_state->viewState());
            return {eff.y_min, eff.y_max};
        });

        // Set Y range to match DataViewer NDC defaults
        _multi_lane_axis_widget->setYRange(
                static_cast<float>(_state->viewState().y_min),
                static_cast<float>(_state->viewState().y_max));

        // Remove OpenGLWidget from its current position in plot_layout
        ui->plot_layout->removeWidget(ui->openGLWidget);

        // Create horizontal layout: [axis widget | OpenGLWidget]
        auto * h_layout = new QHBoxLayout();
        h_layout->setSpacing(0);
        h_layout->setContentsMargins(0, 0, 0, 0);
        h_layout->addWidget(_multi_lane_axis_widget);
        h_layout->addWidget(ui->openGLWidget, 1);// stretch factor 1

        // Insert the HBox at position 0 in the plot_layout (before coordinate_label)
        ui->plot_layout->insertLayout(0, h_layout, 1);// stretch factor 1
    }

    // Connect sceneRebuilt to update lane descriptors
    connect(ui->openGLWidget, &OpenGLWidget::sceneRebuilt,
            this, &DataViewer_Widget::_updateLaneDescriptors);

    // Connect laneReorderRequested to apply lane_order overrides via state
    connect(_multi_lane_axis_widget, &MultiLaneVerticalAxisWidget::laneReorderRequested,
            this, &DataViewer_Widget::_handleLaneReorderRequest);

    // Mirror drag overlay (translucent rect + insertion line) across the OpenGL canvas
    connect(_multi_lane_axis_widget, &MultiLaneVerticalAxisWidget::laneDragOverlayChanged,
            this, [this](bool active, float center, float extent, float marker_y) {
                ui->openGLWidget->setLaneDragOverlay(active, center, extent, marker_y);
            });

    // Connect viewStateChanged to repaint the axis widget (for pan/zoom tracking)
    connect(_state.get(), &DataViewerState::viewStateChanged,
            this, [this]() {
                if (_multi_lane_axis_widget) {
                    _multi_lane_axis_widget->update();
                }
            });

    // Note: Layout computation is now handled by OpenGLWidget's internal LayoutEngine
    // (Phase 4.9 migration - unified layout system)

    // Set up observer to automatically clean up data when it's deleted from DataManager
    // Queue the cleanup to the Qt event loop to avoid running during mid-update mutations
    _data_manager->addObserver([this]() {
        QPointer<DataViewer_Widget> const self = this;
        QMetaObject::invokeMethod(self, [self]() {
            if (!self) return;
            self->cleanupDeletedData(); }, Qt::QueuedConnection);
    });

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
        return;// Don't accept null state
    }

    _state = std::move(state);

    // Share with OpenGLWidget
    ui->openGLWidget->setState(_state);

    // Update axis widget to use the new state's MultiLaneVerticalAxisState
    if (_multi_lane_axis_widget) {
        _multi_lane_axis_widget->setState(_state->multiLaneAxisState());
        _multi_lane_axis_widget->setViewportGetter([this]() -> std::pair<float, float> {
            auto const eff = CorePlotting::computeEffectiveYViewport(_state->viewState());
            return {eff.y_min, eff.y_max};
        });
        _multi_lane_axis_widget->setYRange(
                static_cast<float>(_state->viewState().y_min),
                static_cast<float>(_state->viewState().y_max));
    }

    // Connect viewStateChanged to update labels when time range changes
    connect(_state.get(), &DataViewerState::viewStateChanged, this, [this]() {
        _updateLabels();
    });

    // Connect viewStateChanged to repaint the axis widget
    connect(_state.get(), &DataViewerState::viewStateChanged,
            this, [this]() {
                if (_multi_lane_axis_widget) {
                    _multi_lane_axis_widget->update();
                }
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

void DataViewer_Widget::_onTimeChanged(TimePosition const & position) {
    // Get the TimeFrame for the data this widget is displaying (master clock)
    auto my_tf = _time_frame;

    if (position.sameClock(my_tf)) {
        // Same clock - use index directly
        _state->current_position = position;
        ui->openGLWidget->updateCanvas(position.index);
    } else if (my_tf && position.isValid()) {
        // Different clock - convert
        auto converted = position.convertTo(my_tf);
        _state->current_position = TimePosition(converted, my_tf);
        ui->openGLWidget->updateCanvas(converted);
    }

    _updateLabels();
}

void DataViewer_Widget::_updatePlot(int time) {
    // Note: 'time' is a frame index from the scrollbar, for the "time" time frame
    // This method is kept for backward compatibility but should not be used
    // with the new TimePosition-based system

    if (_time_frame.get() != _data_manager->getTime(TimeKey("time")).get()) {
        auto time_in_ticks = _data_manager->getTime(TimeKey("time"))->getTimeAtIndex(TimeFrameIndex(time));
        TimeFrameIndex const master_index = _time_frame->getIndexAtTime(static_cast<float>(time_in_ticks));
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
        std::string const color = (i < colors.size()) ? colors[i] : "#FF6B6B";// Default color if not provided
        _plotSelectedFeatureWithoutUpdate(key, color);
    }
    _is_batch_add = false;

    // Apply unified group scaling for analog series
    _applyGroupScalingForBatch(keys);

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

    // On some platforms (X11/WSL), holding Alt redirects the vertical scroll
    // delta to the horizontal axis. Use whichever axis has a non-zero value.
    auto const & delta = event->angleDelta();
    int const raw_delta = (delta.y() != 0) ? delta.y() : delta.x();
    auto const numDegrees = static_cast<float>(raw_delta) / 8.0f;
    auto const numSteps = numDegrees / 15.0f;

    bool const has_alt = event->modifiers() & Qt::AltModifier;
    bool const has_shift = event->modifiers() & Qt::ShiftModifier;
    bool const has_ctrl = event->modifiers() & Qt::ControlModifier;

    // ----------------------------------------------------------------
    // Y-axis wheel chords (all require Alt as a common prefix)
    // ----------------------------------------------------------------
    if (has_alt) {
        if (has_shift && !has_ctrl) {
            // Alt+Shift+Scroll → Per-lane Y scale (hovered lane)
            _handlePerLaneYScaleWheel(numSteps, event->position());
        } else if (has_ctrl && !has_shift) {
            // Alt+Ctrl+Scroll → Y viewport zoom (cursor-anchored)
            _handleYViewportZoomWheel(numSteps, event->position());
        } else if (!has_shift && !has_ctrl) {
            // Alt+Scroll → Global Y scale (all lanes)
            _handleGlobalYScaleWheel(numSteps);
        }
        return;
    }

    // ----------------------------------------------------------------
    // X-axis time zoom (existing behavior, unchanged)
    // ----------------------------------------------------------------
    auto const current_range = static_cast<int>(_state->timeWidth());
    auto const total_frames = static_cast<float>(_time_frame->getTotalFrameCount());

    // Determine zoom sensitivity based on modifier keys:
    // - Shift: Fine mode (very small steps for precise adjustment)
    // - Ctrl: Coarse mode (large steps for quick navigation)
    // - None: Normal mode (balanced default behavior)
    bool const fine_mode = has_shift;
    bool const coarse_mode = has_ctrl;

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
            min_clamp_divisor = 10000.0f;// 0.01% of total as minimum
            max_clamp_divisor = 100.0f;  // 1% of total as maximum
        } else if (coarse_mode) {
            // Coarse mode: 20% of current range for quick navigation
            base_percentage = 0.20f;
            min_clamp_divisor = 20.0f;// 5% of total as minimum
            max_clamp_divisor = 5.0f; // 20% of total as maximum
        } else {
            // Normal mode: 3% of current range (reduced from 10% for less sensitivity)
            base_percentage = 0.03f;
            min_clamp_divisor = 1000.0f;// 0.1% of total as minimum
            max_clamp_divisor = 50.0f;  // 2% of total as maximum
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
            divisor = 100000.0f;// Very fine steps
        } else if (coarse_mode) {
            divisor = 1000.0f;// Coarse steps
        } else {
            divisor = 30000.0f;// Normal steps (reduced from 10000 for less sensitivity)
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

void DataViewer_Widget::_handleGlobalYScaleWheel(float numSteps) {
    // Alt+Scroll: adjust global_y_scale (data amplitude gain, affects all lanes)
    // Wheel up → increase scale, wheel down → decrease scale
    float constexpr scale_factor_per_step = 0.05f;
    float constexpr min_scale = 0.01f;
    float constexpr max_scale = 1000.0f;

    float const current = _state->globalYScale();
    float const delta = numSteps * current * scale_factor_per_step;
    float const new_scale = std::clamp(current + delta, min_scale, max_scale);

    _state->setGlobalYScale(new_scale);
}

void DataViewer_Widget::_handlePerLaneYScaleWheel(float numSteps, QPointF const & pos) {
    // Alt+Shift+Scroll: adjust user_scale_factor for the analog series under the cursor
    auto const hit = ui->openGLWidget->findSeriesAtPosition(
            static_cast<float>(pos.x()), static_cast<float>(pos.y()));
    if (!hit) {
        return;
    }

    auto const & [series_type, series_key] = *hit;
    if (series_type != "Analog") {
        return;// Per-lane scaling only applies to analog series
    }

    auto * opts = _state->seriesOptions()
                          .getMutable<AnalogSeriesOptionsData>(QString::fromStdString(series_key));
    if (!opts) {
        return;
    }

    float constexpr scale_factor_per_step = 0.05f;
    float constexpr min_scale = 0.01f;
    float constexpr max_scale = 1000.0f;

    float const delta = numSteps * opts->user_scale_factor * scale_factor_per_step;
    opts->user_scale_factor = std::clamp(opts->user_scale_factor + delta, min_scale, max_scale);

    // Trigger repaint (matches existing pattern in AnalogViewer_Widget)
    ui->openGLWidget->update();
}

void DataViewer_Widget::_handleYViewportZoomWheel(float numSteps, QPointF const & pos) {
    // Alt+Ctrl+Scroll: adjust y_zoom in ViewStateData (projection only, no scene rebuild)
    // Cursor-anchored: the focal point under the mouse remains stable during zoom
    float constexpr zoom_factor_per_step = 0.05f;
    float constexpr min_zoom = 0.1f;
    float constexpr max_zoom = 50.0f;

    auto view = _state->viewState();

    // Compute the world-Y coordinate under the cursor before zoom
    auto const eff_before = CorePlotting::computeEffectiveYViewport(view);
    auto const canvas_height = static_cast<float>(ui->openGLWidget->height());
    float const norm_y = 1.0f - static_cast<float>(pos.y()) / canvas_height;
    float const world_y = eff_before.y_min + norm_y * (eff_before.y_max - eff_before.y_min);

    // Apply zoom
    auto const old_zoom = static_cast<float>(view.y_zoom);
    float const new_zoom = std::clamp(old_zoom * (1.0f + numSteps * zoom_factor_per_step), min_zoom, max_zoom);
    view.y_zoom = static_cast<double>(new_zoom);

    // Adjust y_pan so world_y remains at the same screen position under new zoom
    // From computeEffectiveYViewport: eff_y_min = y_center - (y_range / zoom) / 2 + pan
    // We need: eff_new_y_min + norm_y * (y_range / new_zoom) = world_y
    // => y_center - y_range/(2*new_zoom) + new_pan + norm_y * y_range/new_zoom = world_y
    // => new_pan = world_y - y_center - y_range * (norm_y - 0.5) / new_zoom
    auto const y_range = static_cast<float>(view.y_max - view.y_min);
    float const y_center = static_cast<float>(view.y_min + view.y_max) / 2.0f;
    float const new_pan = world_y - y_center - y_range * (norm_y - 0.5f) / new_zoom;
    view.y_pan = static_cast<double>(new_pan);

    _state->setViewState(view);
}

void DataViewer_Widget::_updateLabels() {
    ui->neg_x_label->setText(QString::number(_state->timeStart()));
    ui->pos_x_label->setText(QString::number(_state->timeEnd()));
}

void DataViewer_Widget::_updateLaneDescriptors() {
    auto * axis_state = _state->multiLaneAxisState();
    auto const & response = ui->openGLWidget->layoutResponse();

    std::map<std::string, LaneAggregate> lanes_by_id;
    for (auto const & layout: response.layouts) {
        if (!isStackableLaneSeries(_state.get(), _data_manager.get(), layout.series_id)) {
            continue;
        }

        std::string const lane_id = laneIdForSeries(_state.get(), layout.series_id);
        auto & lane = lanes_by_id[lane_id];
        lane.lane_id = lane_id;
        lane.series_ids.push_back(layout.series_id);
        lane.y_center = layout.y_transform.offset;
        lane.y_extent = std::abs(layout.y_transform.gain) * 2.0f;
        lane.first_series_index = std::min(lane.first_series_index, layout.series_index);
    }

    std::vector<LaneAggregate> ordered_lanes;
    ordered_lanes.reserve(lanes_by_id.size());
    for (auto & [lane_id, lane]: lanes_by_id) {
        std::sort(lane.series_ids.begin(), lane.series_ids.end());
        ordered_lanes.push_back(std::move(lane));
    }

    std::sort(ordered_lanes.begin(), ordered_lanes.end(),
              [](LaneAggregate const & lhs, LaneAggregate const & rhs) {
                  if (std::abs(lhs.y_center - rhs.y_center) > 1e-6f) {
                      return lhs.y_center < rhs.y_center;
                  }
                  if (lhs.first_series_index != rhs.first_series_index) {
                      return lhs.first_series_index < rhs.first_series_index;
                  }
                  return lhs.lane_id < rhs.lane_id;
              });

    std::vector<LaneAxisDescriptor> descriptors;
    descriptors.reserve(ordered_lanes.size());
    for (int index = 0; index < static_cast<int>(ordered_lanes.size()); ++index) {
        auto const & lane = ordered_lanes[static_cast<size_t>(index)];
        LaneAxisDescriptor descriptor;
        descriptor.label = chooseLaneLabel(_state.get(), _data_manager.get(), lane);
        descriptor.lane_id = lane.lane_id;
        descriptor.y_center = lane.y_center;
        descriptor.y_extent = lane.y_extent;
        descriptor.lane_index = index;
        descriptors.push_back(std::move(descriptor));
    }

    axis_state->setLanes(std::move(descriptors));
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
            bool const in_group = std::find(group_keys.begin(), group_keys.end(), key) != group_keys.end();
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
        _state->setGlobalYScale(final_scale);

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

std::string DataViewer_Widget::_convertDataType(DM_DataType dm_type) {
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

    // Group scaling toggle (only for groups with analog series)
    std::vector<std::string> const group_analog_keys = _findGroupAnalogKeys(group_name);
    if (group_analog_keys.size() >= 2) {
        bool const is_unified = _state->isGroupUnifiedScaling(group_name);
        QAction * unifiedAction = menu.addAction("Unified Std Dev Scaling");
        unifiedAction->setCheckable(true);
        unifiedAction->setChecked(is_unified);

        // Show group std_dev value
        auto const * gs = _state->getGroupScaling(group_name);
        if (gs && gs->group_std_dev > 0.0f) {
            QString const info = QString("Group Std Dev: %1").arg(static_cast<double>(gs->group_std_dev), 0, 'g', 4);
            QAction * infoAction = menu.addAction(info);
            infoAction->setEnabled(false);
        }

        menu.addSeparator();

        connect(unifiedAction, &QAction::toggled, this, [this, group_name](bool checked) {
            _toggleGroupUnifiedScaling(group_name, checked);
        });
    }

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
    QString const path = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("spike_sorter_config"),
            QString("Load spikesorter configuration for %1").arg(group_name),
            "Text Files (*.txt *.cfg *.conf);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QByteArray const data = file.readAll();
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

void DataViewer_Widget::_saveLaneLayout() {
    QString const path = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("lane_layout_json"),
            "Save Lane Layout",
            "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    // Collect currently-displayed series and their colors from the series options registry
    LaneLayoutFile layout_file;
    auto const & opts = _state->seriesOptions();

    for (auto const & qkey: opts.keys<AnalogSeriesOptionsData>()) {
        auto const * series_opts = opts.get<AnalogSeriesOptionsData>(qkey);
        if (series_opts != nullptr) {
            layout_file.displayed_series.push_back(
                    LaneLayoutDisplayedSeries{qkey.toStdString(), series_opts->hex_color()});
        }
    }
    for (auto const & qkey: opts.keys<DigitalEventSeriesOptionsData>()) {
        auto const * series_opts = opts.get<DigitalEventSeriesOptionsData>(qkey);
        if (series_opts != nullptr) {
            layout_file.displayed_series.push_back(
                    LaneLayoutDisplayedSeries{qkey.toStdString(), series_opts->hex_color()});
        }
    }
    for (auto const & qkey: opts.keys<DigitalIntervalSeriesOptionsData>()) {
        auto const * series_opts = opts.get<DigitalIntervalSeriesOptionsData>(qkey);
        if (series_opts != nullptr) {
            layout_file.displayed_series.push_back(
                    LaneLayoutDisplayedSeries{qkey.toStdString(), series_opts->hex_color()});
        }
    }

    // Copy lane placement overrides from state
    auto const & d = _state->data();
    layout_file.series_lane_overrides = d.series_lane_overrides;
    layout_file.lane_overrides = d.lane_overrides;
    layout_file.ordering_constraints = d.ordering_constraints;

    std::string const json = rfl::json::write(layout_file);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Save Failed", "Could not open file for writing.");
        return;
    }
    file.write(QByteArray::fromStdString(json));
}

void DataViewer_Widget::_loadLaneLayout() {
    QString const path = AppFileDialog::getOpenFileName(
            this,
            QStringLiteral("lane_layout_json"),
            "Load Lane Layout",
            "JSON Files (*.json);;All Files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Load Failed", "Could not open file for reading.");
        return;
    }

    std::string const json = file.readAll().toStdString();
    auto result = rfl::json::read<LaneLayoutFile>(json);
    if (!result) {
        QMessageBox::warning(this, "Parse Failed", "Could not parse lane layout JSON.");
        return;
    }

    auto const & layout = result.value();

    // Add series that exist in DataManager but are not currently displayed
    for (auto const & entry: layout.displayed_series) {
        DM_DataType const type = _data_manager->getType(entry.key);
        if (type == DM_DataType::Unknown) {
            continue;// silently skip missing series
        }

        // Skip series that are already displayed (check options registry)
        auto const qkey = QString::fromStdString(entry.key);
        bool const already_displayed =
                _state->seriesOptions().has<AnalogSeriesOptionsData>(qkey) ||
                _state->seriesOptions().has<DigitalEventSeriesOptionsData>(qkey) ||
                _state->seriesOptions().has<DigitalIntervalSeriesOptionsData>(qkey);
        if (already_displayed) {
            continue;
        }

        addFeature(entry.key, entry.color);
    }

    // Apply all lane placement overrides
    for (auto const & [key, od]: layout.series_lane_overrides) {
        _state->setSeriesLaneOverride(key, od);
    }
    for (auto const & [lane_id, od]: layout.lane_overrides) {
        _state->setLaneOverride(lane_id, od);
    }
    _state->setOrderingConstraints(layout.ordering_constraints);

    ui->openGLWidget->updateCanvas();
}

std::vector<ChannelPosition> DataViewer_Widget::_parseSpikeSorterConfig(std::string const & text) {
    return parseSwindaleSpikeSorterConfig(text);
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

void DataViewer_Widget::_loadSpikeToAnalogPairing() {
    SpikeToAnalogConfigDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    QString const file_path = QString::fromStdString(dlg.filePath());
    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    QString const text = stream.readAll();

    SpikeToAnalogParseConfig config;
    config.delimiter = dlg.delimiter();
    config.digital_column = dlg.digitalColumn();
    config.analog_column = dlg.analogColumn();
    config.digital_one_based = dlg.digitalOneBased();
    config.analog_one_based = dlg.analogOneBased();
    config.digital_key_one_based = dlg.digitalKeyOneBased();
    config.analog_key_one_based = dlg.analogKeyOneBased();

    _applyAnalogPairing(
            dlg.digitalGroupPrefix(),
            dlg.analogGroupPrefix(),
            text.toStdString(),
            dlg.placementMode(),
            config);
}

void DataViewer_Widget::_loadSpikeToAnalogPairingFromText(QString const & digital_group,
                                                          QString const & analog_group,
                                                          QString const & text,
                                                          int placement_mode) {
    _applyAnalogPairing(
            digital_group.toStdString(),
            analog_group.toStdString(),
            text.toStdString(),
            static_cast<SpikeToAnalogPlacementMode>(placement_mode),
            SpikeToAnalogParseConfig{});
}

void DataViewer_Widget::_applyAnalogPairing(std::string const & digital_group,
                                            std::string const & analog_group,
                                            std::string const & text,
                                            SpikeToAnalogPlacementMode mode,
                                            SpikeToAnalogParseConfig const & config) {
    auto const pairings = parseSpikeToAnalogCSV(text, config);
    if (pairings.empty()) {
        return;
    }

    ui->openGLWidget->loadSpikeToAnalogPairing(digital_group, analog_group, pairings, mode, config);
    ui->openGLWidget->updateCanvas();
}

void DataViewer_Widget::_handleLaneReorderRequest(QString const & source_lane_id, int target_visual_slot) {
    auto * axis_state = _state->multiLaneAxisState();
    if (axis_state == nullptr) {
        return;
    }

    auto const & lanes = axis_state->lanes();
    int const n = static_cast<int>(lanes.size());
    if (n <= 1) {
        return;// Nothing to reorder
    }

    // Build the current visual order (top-to-bottom = NDC descending = reverse of lanes vector).
    // visual_order[i] = lane_id at visual position i (0 = top).
    std::vector<std::string> visual_order;
    visual_order.reserve(static_cast<size_t>(n));
    for (int i = n - 1; i >= 0; --i) {
        visual_order.push_back(lanes[static_cast<size_t>(i)].lane_id);
    }

    // Find the source lane in visual order.
    std::string const src = source_lane_id.toStdString();
    auto const src_it = std::find(visual_order.begin(), visual_order.end(), src);
    if (src_it == visual_order.end()) {
        return;// Unknown lane_id — stale descriptor
    }
    int const src_visual_idx = static_cast<int>(src_it - visual_order.begin());

    // Build new visual order: remove source and insert at target slot.
    std::vector<std::string> new_visual_order;
    new_visual_order.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (i != src_visual_idx) {
            new_visual_order.push_back(visual_order[static_cast<size_t>(i)]);
        }
    }
    int const clamped_slot = std::clamp(target_visual_slot, 0, n - 1);
    new_visual_order.insert(new_visual_order.begin() + clamped_slot, src);

    if (new_visual_order == visual_order) {
        return;// No-op: dropped back to the same position
    }

    // Assign new lane_order values.
    // Visual slot 0 (top) receives the highest lane_order; slot N-1 (bottom) receives the lowest.
    // lane_order = (N - visual_idx) * 10
    constexpr int kLaneOrderStep = 10;

    // Snapshot the current overrides map before making changes to avoid iterator invalidation.
    auto const overrides_snapshot = _state->allSeriesLaneOverrides();

    struct LaneUpdate {
        std::string series_key;
        SeriesLaneOverrideData updated;
    };
    std::vector<LaneUpdate> updates;

    std::string const auto_prefix = "__auto_lane__";
    for (int visual_idx = 0; visual_idx < n; ++visual_idx) {
        std::string const & lid = new_visual_order[static_cast<size_t>(visual_idx)];
        int const new_order = (n - visual_idx) * kLaneOrderStep;

        if (lid.rfind(auto_prefix, 0) == 0) {
            // Auto-lane: the series key is the suffix after "__auto_lane__".
            // Create an explicit override so the order persists.
            std::string const series_key = lid.substr(auto_prefix.size());
            SeriesLaneOverrideData new_override;
            new_override.lane_id = lid;
            new_override.lane_order = new_order;
            updates.push_back({series_key, new_override});
        } else {
            // Explicit lane: update all series whose lane_id matches.
            for (auto const & [key, od]: overrides_snapshot) {
                if (od.lane_id == lid) {
                    SeriesLaneOverrideData updated = od;
                    updated.lane_order = new_order;
                    updates.push_back({key, updated});
                }
            }
        }
    }

    for (auto & upd: updates) {
        _state->setSeriesLaneOverride(upd.series_key, upd.updated);
    }
}

void DataViewer_Widget::_handleSeriesRelativePlacement(QString const & source_key,
                                                       QString const & target_key,
                                                       bool above) {
    auto * axis_state = _state->multiLaneAxisState();
    if (axis_state == nullptr) {
        return;
    }

    auto const & lanes = axis_state->lanes();
    int const n = static_cast<int>(lanes.size());
    if (n <= 1) {
        return;
    }

    // Build current visual order (top-to-bottom = NDC descending = reverse of lanes vector).
    std::vector<std::string> visual_order;
    visual_order.reserve(static_cast<size_t>(n));
    for (int i = n - 1; i >= 0; --i) {
        visual_order.push_back(lanes[static_cast<size_t>(i)].lane_id);
    }

    // Resolve lane_ids for source and target series.
    // A series without an explicit lane_id lives in an auto-lane named "__auto_lane__<key>".
    std::string const auto_prefix = "__auto_lane__";
    auto const resolve_lane_id = [&](std::string const & key) -> std::string {
        if (auto const * od = _state->getSeriesLaneOverride(key);
            od != nullptr && !od->lane_id.empty()) {
            return od->lane_id;
        }
        return auto_prefix + key;
    };

    std::string const src_lid = resolve_lane_id(source_key.toStdString());
    std::string const tgt_lid = resolve_lane_id(target_key.toStdString());

    auto const src_it = std::find(visual_order.begin(), visual_order.end(), src_lid);
    auto const tgt_it = std::find(visual_order.begin(), visual_order.end(), tgt_lid);

    if (src_it == visual_order.end() || tgt_it == visual_order.end()) {
        return;// Unknown lane — stale descriptors
    }

    int const src_visual_idx = static_cast<int>(src_it - visual_order.begin());
    int const tgt_visual_idx = static_cast<int>(tgt_it - visual_order.begin());

    // Build new visual order: remove source, insert adjacent to target.
    // Insert at tgt_visual_idx for "above", tgt_visual_idx+1 for "below"
    // (adjusting for the removal of source which may shift indices).
    std::vector<std::string> new_visual_order;
    new_visual_order.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (i != src_visual_idx) {
            new_visual_order.push_back(visual_order[static_cast<size_t>(i)]);
        }
    }

    // After removal, find where the target now sits.
    auto const new_tgt_it = std::find(new_visual_order.begin(), new_visual_order.end(), tgt_lid);
    if (new_tgt_it == new_visual_order.end()) {
        return;
    }
    int const new_tgt_idx = static_cast<int>(new_tgt_it - new_visual_order.begin());
    int const insert_slot = above ? new_tgt_idx : new_tgt_idx + 1;
    new_visual_order.insert(new_visual_order.begin() + insert_slot, src_lid);

    if (new_visual_order == visual_order) {
        return;// No-op
    }

    // Assign new lane_order values: visual slot 0 (top) = highest value.
    constexpr int kLaneOrderStep = 10;
    auto const overrides_snapshot = _state->allSeriesLaneOverrides();

    struct LaneUpdate {
        std::string series_key;
        SeriesLaneOverrideData updated;
    };
    std::vector<LaneUpdate> updates;

    for (int visual_idx = 0; visual_idx < n; ++visual_idx) {
        std::string const & lid = new_visual_order[static_cast<size_t>(visual_idx)];
        int const new_order = (n - visual_idx) * kLaneOrderStep;

        if (lid.rfind(auto_prefix, 0) == 0) {
            std::string const series_key = lid.substr(auto_prefix.size());
            SeriesLaneOverrideData new_override;
            new_override.lane_id = lid;
            new_override.lane_order = new_order;
            updates.push_back({series_key, new_override});
        } else {
            for (auto const & [key, od]: overrides_snapshot) {
                if (od.lane_id == lid) {
                    SeriesLaneOverrideData updated = od;
                    updated.lane_order = new_order;
                    updates.push_back({key, updated});
                }
            }
        }
    }

    for (auto & upd: updates) {
        _state->setSeriesLaneOverride(upd.series_key, upd.updated);
    }
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

    int const visible_analog_count = static_cast<int>(analog_keys.size());
    int const visible_event_count = static_cast<int>(event_keys.size());
    int const visible_interval_count = static_cast<int>(interval_keys.size());

    int const total_visible = visible_analog_count + visible_event_count + visible_interval_count;
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
        // Analog data is normalized by std-dev in the transform pipeline.
        // Target a fraction of each lane's height directly rather than dividing
        // by measured std-dev again (which can significantly underscale traces).
        float const target_amplitude_fraction = 0.6f;
        float const margin_factor = std::clamp(_state->marginFactor(), 0.05f, 1.0f);
        float const optimal_global_scale = target_amplitude_fraction / margin_factor;

        // Clamp to reasonable bounds
        float const min_scale = 0.05f;
        float const max_scale = 20.0f;
        float const final_scale = std::clamp(optimal_global_scale, min_scale, max_scale);

        std::cout << "Calculated optimal global scale: target_fraction=" << target_amplitude_fraction
                  << ", margin_factor=" << margin_factor
                  << ", final_scale=" << final_scale << std::endl;

        // Apply the calculated global scale
        _state->setGlobalYScale(final_scale);
    }

    std::cout << "Auto-fill canvas completed" << std::endl;
}

void DataViewer_Widget::_applyGroupScalingForBatch(std::vector<std::string> const & keys) {
    // Filter to only analog series keys
    std::vector<std::string> analog_keys;
    for (auto const & key: keys) {
        if (_data_manager->getType(key) == DM_DataType::Analog) {
            analog_keys.push_back(key);
        }
    }
    if (analog_keys.size() < 2) {
        return;// No point in group scaling for 0 or 1 series
    }

    // Derive group name from the first key
    std::string const group_name = extractGroupName(analog_keys[0]);
    if (group_name.empty()) {
        return;
    }

    // Compute max std_dev across the group using data store caches
    auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
    float max_std_dev = 0.0f;
    for (auto const & key: analog_keys) {
        auto it = analog_map.find(key);
        if (it != analog_map.end()) {
            float const sd = it->second.data_cache.individual_std_dev;
            if (sd > max_std_dev) {
                max_std_dev = sd;
            }
        }
    }

    if (max_std_dev <= 0.0f) {
        return;
    }

    // Check if unified scaling is enabled for this group (default: true)
    bool const unified = _state->isGroupUnifiedScaling(group_name);

    // Store group scaling state
    GroupScalingState gs;
    gs.unified_scaling = unified;
    gs.group_std_dev = max_std_dev;
    _state->setGroupScaling(group_name, gs);

    if (unified) {
        // Apply the group std_dev to all analog series in the batch
        // Access the data store through OpenGLWidget to modify caches
        for (auto const & key: analog_keys) {
            auto * cache = const_cast<CorePlotting::SeriesDataCache *>(
                    ui->openGLWidget->getAnalogSeriesMap().count(key) > 0
                            ? &ui->openGLWidget->getAnalogSeriesMap().at(key).data_cache
                            : nullptr);
            if (cache) {
                cache->cached_std_dev = max_std_dev;
                cache->std_dev_cache_valid = true;
                cache->intrinsic_scale = 1.0f / (3.0f * max_std_dev);
            }
        }
        std::cout << "Applied unified group scaling for '" << group_name
                  << "': group_std_dev=" << max_std_dev
                  << " across " << analog_keys.size() << " series" << std::endl;
    }
}

void DataViewer_Widget::_toggleGroupUnifiedScaling(std::string const & group_name, bool enabled) {
    std::vector<std::string> const group_keys = _findGroupAnalogKeys(group_name);
    if (group_keys.empty()) {
        return;
    }

    auto * gs = _state->getGroupScalingMutable(group_name);
    if (!gs) {
        // Create new state
        GroupScalingState new_gs;
        new_gs.unified_scaling = enabled;
        // Compute max std_dev
        auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
        float max_std_dev = 0.0f;
        for (auto const & key: group_keys) {
            auto it = analog_map.find(key);
            if (it != analog_map.end()) {
                float const sd = it->second.data_cache.individual_std_dev;
                if (sd > max_std_dev) {
                    max_std_dev = sd;
                }
            }
        }
        new_gs.group_std_dev = max_std_dev;
        _state->setGroupScaling(group_name, new_gs);
        gs = _state->getGroupScalingMutable(group_name);
    } else {
        gs->unified_scaling = enabled;
    }

    if (enabled && gs != nullptr) {
        // Apply group std_dev to all series in the group
        for (auto const & key: group_keys) {
            auto * cache = const_cast<CorePlotting::SeriesDataCache *>(
                    ui->openGLWidget->getAnalogSeriesMap().count(key) > 0
                            ? &ui->openGLWidget->getAnalogSeriesMap().at(key).data_cache
                            : nullptr);
            if (cache) {
                cache->cached_std_dev = gs->group_std_dev;
                cache->std_dev_cache_valid = true;
                cache->intrinsic_scale = 1.0f / (3.0f * gs->group_std_dev);
            }
        }
        std::cout << "Enabled unified scaling for group '" << group_name
                  << "': std_dev=" << gs->group_std_dev << std::endl;
    } else {
        // Revert to individual std_devs
        for (auto const & key: group_keys) {
            auto * cache = const_cast<CorePlotting::SeriesDataCache *>(
                    ui->openGLWidget->getAnalogSeriesMap().count(key) > 0
                            ? &ui->openGLWidget->getAnalogSeriesMap().at(key).data_cache
                            : nullptr);
            if (cache) {
                cache->cached_std_dev = cache->individual_std_dev;
                cache->std_dev_cache_valid = true;
                if (cache->individual_std_dev > 0.0f) {
                    cache->intrinsic_scale = 1.0f / (3.0f * cache->individual_std_dev);
                } else {
                    cache->intrinsic_scale = 1.0f;
                }
            }
        }
        std::cout << "Disabled unified scaling for group '" << group_name
                  << "', reverted to individual std_devs" << std::endl;
    }

    // Trigger re-render
    ui->openGLWidget->updateCanvas();
}

std::vector<std::string> DataViewer_Widget::_findGroupAnalogKeys(std::string const & group_name) const {
    std::vector<std::string> result;
    auto const & analog_map = ui->openGLWidget->getAnalogSeriesMap();
    for (auto const & [key, entry]: analog_map) {
        if (extractGroupName(key) == group_name) {
            result.push_back(key);
        }
    }
    return result;
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
    QPointer<OpenGLWidget> const glw = ui ? ui->openGLWidget : nullptr;
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
    QString const fileName = AppFileDialog::getSaveFileName(
            this,
            QStringLiteral("export_svg"),
            tr("Export Plot to SVG"),
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
