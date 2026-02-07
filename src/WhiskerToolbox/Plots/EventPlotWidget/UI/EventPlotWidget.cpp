#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisSynchronizer.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_EventPlotWidget.h"

EventPlotWidget::EventPlotWidget(std::shared_ptr<DataManager> data_manager,
                                 QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::EventPlotWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_axis_state(nullptr) {
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Create vertical axis state (not serialized in EventPlotState since Y-axis is trial-based)
    _vertical_axis_state = std::make_unique<VerticalAxisState>(this);

    // Create combined vertical axis widget with range controls using factory
    // Range controls will be created in the properties widget
    auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(
            _vertical_axis_state.get(), this, nullptr);
    _vertical_axis_widget = vertical_axis_with_controls.axis_widget;
    _vertical_range_controls = vertical_axis_with_controls.range_controls;
    _vertical_axis_state->setRange(0.0, 0.0);// Will be updated when trials are loaded
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);// Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);// Stretch factor 1

    // Time axis widget and controls will be created in setState()
    // when we have access to the EventPlotState's RelativeTimeAxisState
    _axis_widget = nullptr;
    _range_controls = nullptr;

    // Replace the main layout
    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventDoubleClicked,
            this, [this](int64_t time_frame_index, QString const & series_key) {
                // Get the TimeFrame from the series via DataManager
                std::shared_ptr<TimeFrame> time_frame;
                if (_data_manager && !series_key.isEmpty()) {
                    auto time_key = _data_manager->getTimeKey(series_key.toStdString());
                    if (!time_key.empty()) {
                        time_frame = _data_manager->getTime(time_key);
                    }
                }
                emit timePositionSelected(TimePosition(TimeFrameIndex(time_frame_index), time_frame));
            });

    // Forward event selection signal
    connect(_opengl_widget, &EventPlotOpenGLWidget::eventSelected,
            this, &EventPlotWidget::eventSelected);

    // Connect trial count changes to cache the count
    // The actual range is computed by the RangeGetter set up in setState()
    connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                _trial_count = count;
                if (_vertical_axis_widget) {
                    _vertical_axis_widget->update();
                }
            });
}

EventPlotWidget::~EventPlotWidget() {
    delete ui;
}

void EventPlotWidget::setState(std::shared_ptr<EventPlotState> state) {
    _state = state;

    // Pass state to OpenGL widget
    if (_opengl_widget) {
        _opengl_widget->setState(_state);
    }

    // Create time axis widget and controls using factory
    // This must be done after setState so we have access to RelativeTimeAxisState
    if (_state && !_axis_widget) {
        auto * time_axis_state = _state->relativeTimeAxisState();
        if (time_axis_state) {
            auto time_axis_with_controls = createRelativeTimeAxisWithRangeControls(
                time_axis_state, this, nullptr);
            _axis_widget = time_axis_with_controls.axis_widget;
            _range_controls = time_axis_with_controls.range_controls;

            // Add time axis widget to layout
            if (_axis_widget) {
                QLayout * main_layout = layout();
                if (main_layout) {
                    QVBoxLayout * vbox = qobject_cast<QVBoxLayout *>(main_layout);
                    if (vbox) {
                        vbox->addWidget(_axis_widget);
                    }
                }
            }
        }
    }

    // Set up axis widget with ViewState getter and connect to state changes
    if (_axis_widget && _state) {
        // Set up the ViewState getter function
        _axis_widget->setViewStateGetter([this]() {
            if (!_state || !_opengl_widget) {
                return CorePlotting::ViewState{};
            }
            auto const & event_view_state = _state->viewState();
            // Get viewport size from OpenGL widget
            int width = _opengl_widget->width();
            int height = _opengl_widget->height();
            return toCoreViewState(event_view_state, width, height);
        });

        // Connect to view state changes (emitted when window size, zoom, pan, or bounds change)
        _axis_widget->connectToViewStateChanged(
                _state.get(),
                &EventPlotState::viewStateChanged);

        // Also connect to OpenGL widget's viewBoundsChanged signal
        // This ensures the axis widget updates when the OpenGL widget's view changes
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                _axis_widget, [this]() {
                    if (_axis_widget) {
                        _axis_widget->update();
                    }
                });
    }

    // Connect time axis state to update view bounds when user changes range
    // This is handled in EventPlotState, but we also need to update visible range
    // when view state changes (from panning/zooming)
    if (_state) {
        auto * time_axis_state = _state->relativeTimeAxisState();
        if (time_axis_state) {
            // Update time axis range when view bounds change (from panning/zooming)
            auto updateRangeFromViewState = [this, time_axis_state]() {
                if (_state && _opengl_widget) {
                    // Compute visible bounds from view state
                    auto const & view_state = _state->viewState();
                    // Visible range = data bounds / zoom + pan
                    double const zoomed_half_range = (view_state.x_max - view_state.x_min) / 2.0 / view_state.x_zoom;
                    double const visible_min = (view_state.x_min + view_state.x_max) / 2.0 - zoomed_half_range + view_state.x_pan;
                    double const visible_max = (view_state.x_min + view_state.x_max) / 2.0 + zoomed_half_range + view_state.x_pan;
                    // Update time axis state (this will update spinboxes without triggering rangeChanged)
                    time_axis_state->setRangeSilent(visible_min, visible_max);
                }
            };

            connect(_state.get(), &EventPlotState::viewStateChanged,
                    this, updateRangeFromViewState);

            // Also connect to OpenGL widget's viewBoundsChanged signal
            connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                    this, updateRangeFromViewState);

            // Initialize time axis range from current view bounds
            updateRangeFromViewState();
        }
    }

    // Set up vertical axis with RangeGetter that computes visible trial range
    // based on Y zoom and pan from the view state
    if (_vertical_axis_widget && _state) {

        // When trial count changes, update the axis mapping so labels/conversions are correct
        connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
                this, [this](size_t count) {
                    if (count > 0) {
                        _vertical_axis_widget->setAxisMapping(
                            CorePlotting::trialIndexAxis(count));
                    }
                });

        // Set initial mapping if trial count is known
        if (_trial_count > 0) {
            _vertical_axis_widget->setAxisMapping(
                CorePlotting::trialIndexAxis(_trial_count));
        }

        _vertical_axis_widget->setRangeGetter([this]() -> std::pair<double, double> {
            if (_trial_count == 0) {
                return {0.0, 0.0};
            }

            auto const & view_state = _state->viewState();
            auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

            // Y world coordinates span [-1, 1] (full range)
            // With zoom/pan, the visible range becomes:
            //   visible_bottom = -1.0 / y_zoom + y_pan
            //   visible_top = 1.0 / y_zoom + y_pan
            double const zoomed_half_range = 1.0 / view_state.y_zoom;
            double const visible_bottom = -zoomed_half_range + view_state.y_pan;
            double const visible_top = zoomed_half_range + view_state.y_pan;

            double const min_trial = mapping.worldToDomain(visible_bottom);
            double const max_trial = mapping.worldToDomain(visible_top);

            // Return visible range without clamping - axis should show
            // coordinates even when panned/zoomed beyond data bounds
            return {
                    std::min(min_trial, max_trial),
                    std::max(min_trial, max_trial)};
        });

        // Connect to view state changes so vertical axis updates on zoom/pan
        _vertical_axis_widget->connectToRangeChanged(
                _state.get(),
                &EventPlotState::viewStateChanged);

        // Also connect to OpenGL widget's viewBoundsChanged signal
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                _vertical_axis_widget, [this]() {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });
    }

    // Connect vertical axis state to update view state when user changes range
    if (_vertical_axis_state && _state) {
        connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    // Update the view state y_zoom and y_pan when user changes range
                    if (_state && _trial_count > 0) {
                        // Convert trial indices to world Y coordinates
                        auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

                        double world_y_min = mapping.domainToWorld(min_range);
                        double world_y_max = mapping.domainToWorld(max_range);

                        // Compute y_zoom and y_pan to show the desired range
                        // We want: visible_bottom = world_y_min, visible_top = world_y_max
                        // Where: visible_bottom = -1.0 / y_zoom + y_pan
                        //        visible_top = 1.0 / y_zoom + y_pan
                        // Solving: y_zoom = 2.0 / (world_y_max - world_y_min)
                        //          y_pan = (world_y_min + world_y_max) / 2.0
                        double world_range = world_y_max - world_y_min;
                        if (world_range > 0.001) {// Avoid division by zero
                            double new_y_zoom = 2.0 / world_range;
                            double new_y_pan = (world_y_min + world_y_max) / 2.0;
                            _state->setYZoom(new_y_zoom);
                            _state->setPan(_state->viewState().x_pan, new_y_pan);
                        }
                    }
                });

        // Set up bidirectional synchronization: ViewState -> AxisState (Flow B)
        // When ViewState changes (from panning/zooming), update AxisState silently
        syncVerticalAxisToViewState(
                _vertical_axis_state.get(),
                _state.get(),
                [this](EventPlotViewState const & view_state) -> std::pair<double, double> {
                    if (_trial_count == 0) {
                        return {0.0, 0.0};
                    }

                    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

                    // Compute visible trial range from view state
                    double const zoomed_half_range = 1.0 / view_state.y_zoom;
                    double const visible_bottom = -zoomed_half_range + view_state.y_pan;
                    double const visible_top = zoomed_half_range + view_state.y_pan;

                    double const min_trial = mapping.worldToDomain(visible_bottom);
                    double const max_trial = mapping.worldToDomain(visible_top);

                    return {
                            std::min(min_trial, max_trial),
                            std::max(min_trial, max_trial)};
                });

        // Also connect to OpenGL widget's viewBoundsChanged signal for immediate updates
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                _vertical_axis_widget, [this]() {
                    if (_vertical_axis_widget) {
                        _vertical_axis_widget->update();
                    }
                });

        // Initialize vertical range state from current view bounds
        if (_state && _vertical_axis_state && _trial_count > 0) {
            auto const & view_state = _state->viewState();
            auto const mapping = CorePlotting::trialIndexAxis(_trial_count);
            double const zoomed_half_range = 1.0 / view_state.y_zoom;
            double const visible_bottom = -zoomed_half_range + view_state.y_pan;
            double const visible_top = zoomed_half_range + view_state.y_pan;
            double const min_trial = mapping.worldToDomain(visible_bottom);
            double const max_trial = mapping.worldToDomain(visible_top);
            _vertical_axis_state->setRangeSilent(
                    std::min(min_trial, max_trial),
                    std::max(min_trial, max_trial));
        }
    }
}

void EventPlotWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    // Update axis widget when widget resizes to ensure it gets fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
}

EventPlotState * EventPlotWidget::state() {
    return _state.get();
}

RelativeTimeAxisRangeControls * EventPlotWidget::getRangeControls() const {
    return _range_controls;
}


VerticalAxisRangeControls * EventPlotWidget::getVerticalRangeControls() const {
    return _vertical_range_controls;
}

VerticalAxisState * EventPlotWidget::getVerticalAxisState() const {
    return _vertical_axis_state.get();
}
