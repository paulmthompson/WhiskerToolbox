#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"

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
      _range_state(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_state(nullptr)
{
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Create combined vertical axis widget with range controls using factory
    // Range controls will be created in the properties widget
    auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(this, nullptr);
    _vertical_axis_widget = vertical_axis_with_controls.axis_widget;
    _vertical_range_controls = vertical_axis_with_controls.range_controls;
    _vertical_range_state = vertical_axis_with_controls.state;
    _vertical_axis_widget->setRange(0.0, 0.0);  // Will be updated when trials are loaded
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new EventPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);  // Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);  // Stretch factor 1

    // Create combined axis widget with range controls using factory
    // Range controls will be created in the properties widget
    auto axis_with_controls = createRelativeTimeAxisWithRangeControls(this, nullptr);
    _axis_widget = axis_with_controls.axis_widget;
    _range_controls = axis_with_controls.range_controls;
    _range_state = axis_with_controls.state;
    vertical_layout->addWidget(_axis_widget);

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

EventPlotWidget::~EventPlotWidget()
{
    delete ui;
}

void EventPlotWidget::setState(std::shared_ptr<EventPlotState> state)
{
    _state = state;
    
    // Pass state to OpenGL widget
    if (_opengl_widget) {
        _opengl_widget->setState(_state);
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

    // Connect range state to update view bounds when user changes range
    if (_range_state && _state) {
        connect(_range_state.get(), &RelativeTimeAxisRangeState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    // Update the view state bounds when user changes range
                    if (_state) {
                        _state->setXBounds(min_range, max_range);
                    }
                });

        // Update range state when view bounds change (from panning/zooming)
        auto updateRangeFromViewState = [this]() {
            if (_range_state && _state && _opengl_widget) {
                // Compute visible bounds from view state
                auto const & view_state = _state->viewState();
                // Visible range = data bounds / zoom + pan
                double const zoomed_half_range = (view_state.x_max - view_state.x_min) / 2.0 / view_state.x_zoom;
                double const visible_min = (view_state.x_min + view_state.x_max) / 2.0 - zoomed_half_range + view_state.x_pan;
                double const visible_max = (view_state.x_min + view_state.x_max) / 2.0 + zoomed_half_range + view_state.x_pan;
                // Update range state (this will update combo boxes without triggering rangeChanged)
                _range_state->setRange(visible_min, visible_max);
            }
        };

        connect(_state.get(), &EventPlotState::viewStateChanged,
                this, updateRangeFromViewState);

        // Also connect to OpenGL widget's viewBoundsChanged signal
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                this, updateRangeFromViewState);

        // Initialize range state from current view bounds
        updateRangeFromViewState();
    }

    // Set up vertical axis with RangeGetter that computes visible trial range
    // based on Y zoom and pan from the view state
    if (_vertical_axis_widget && _state) {
        _vertical_axis_widget->setRangeGetter([this]() -> std::pair<double, double> {
            if (_trial_count == 0) {
                return {0.0, 0.0};
            }

            auto const & view_state = _state->viewState();

            // Y world coordinates span [-1, 1] (full range)
            // With zoom/pan, the visible range becomes:
            //   visible_bottom = -1.0 / y_zoom + y_pan
            //   visible_top = 1.0 / y_zoom + y_pan
            double const zoomed_half_range = 1.0 / view_state.y_zoom;
            double const visible_bottom = -zoomed_half_range + view_state.y_pan;
            double const visible_top = zoomed_half_range + view_state.y_pan;

            // Map world Y [-1, 1] to trial index [0, N-1]
            // Row layout places trial 0 at bottom (Y = -1 + epsilon)
            // and trial N-1 at top (Y = 1 - epsilon)
            // Mapping: trial = (world_y + 1) / 2 * trial_count
            auto world_y_to_trial = [this](double world_y) {
                return (world_y + 1.0) / 2.0 * static_cast<double>(_trial_count);
            };

            double const min_trial = world_y_to_trial(visible_bottom);
            double const max_trial = world_y_to_trial(visible_top);

            // Return visible range without clamping - axis should show
            // coordinates even when panned/zoomed beyond data bounds
            return {
                std::min(min_trial, max_trial),
                std::max(min_trial, max_trial)
            };
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

    // Connect vertical range state to update view state when user changes range
    if (_vertical_range_state && _state) {
        connect(_vertical_range_state.get(), &VerticalAxisRangeState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    // Update the view state y_zoom and y_pan when user changes range
                    if (_state && _trial_count > 0) {
                        // Convert trial indices to world Y coordinates
                        auto trial_to_world_y = [this](double trial) {
                            return (trial / static_cast<double>(_trial_count)) * 2.0 - 1.0;
                        };

                        double world_y_min = trial_to_world_y(min_range);
                        double world_y_max = trial_to_world_y(max_range);

                        // Compute y_zoom and y_pan to show the desired range
                        // We want: visible_bottom = world_y_min, visible_top = world_y_max
                        // Where: visible_bottom = -1.0 / y_zoom + y_pan
                        //        visible_top = 1.0 / y_zoom + y_pan
                        // Solving: y_zoom = 2.0 / (world_y_max - world_y_min)
                        //          y_pan = (world_y_min + world_y_max) / 2.0
                        double world_range = world_y_max - world_y_min;
                        if (world_range > 0.001) {  // Avoid division by zero
                            double new_y_zoom = 2.0 / world_range;
                            double new_y_pan = (world_y_min + world_y_max) / 2.0;
                            _state->setYZoom(new_y_zoom);
                            _state->setPan(_state->viewState().x_pan, new_y_pan);
                        }
                    }
                });

        // Update vertical range state when view bounds change (from panning/zooming)
        auto updateVerticalRangeFromViewState = [this]() {
            if (_vertical_range_state && _state && _trial_count > 0) {
                auto const & view_state = _state->viewState();

                // Compute visible trial range from view state
                double const zoomed_half_range = 1.0 / view_state.y_zoom;
                double const visible_bottom = -zoomed_half_range + view_state.y_pan;
                double const visible_top = zoomed_half_range + view_state.y_pan;

                // Map world Y to trial index
                auto world_y_to_trial = [this](double world_y) {
                    return (world_y + 1.0) / 2.0 * static_cast<double>(_trial_count);
                };

                double const min_trial = world_y_to_trial(visible_bottom);
                double const max_trial = world_y_to_trial(visible_top);

                // Update range state (this will update combo boxes without triggering rangeChanged)
                _vertical_range_state->setRange(
                    std::min(min_trial, max_trial),
                    std::max(min_trial, max_trial));
            }
        };

        connect(_state.get(), &EventPlotState::viewStateChanged,
                this, updateVerticalRangeFromViewState);

        // Also connect to OpenGL widget's viewBoundsChanged signal
        connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
                this, updateVerticalRangeFromViewState);

        // Initialize vertical range state from current view bounds
        updateVerticalRangeFromViewState();
    }
}

void EventPlotWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    // Update axis widget when widget resizes to ensure it gets fresh viewport dimensions
    if (_axis_widget) {
        _axis_widget->update();
    }
}

EventPlotState * EventPlotWidget::state()
{
    return _state.get();
}

RelativeTimeAxisRangeControls * EventPlotWidget::getRangeControls() const
{
    return _range_controls;
}

std::shared_ptr<RelativeTimeAxisRangeState> EventPlotWidget::getRangeState() const
{
    return _range_state;
}

VerticalAxisRangeControls * EventPlotWidget::getVerticalRangeControls() const
{
    return _vertical_range_controls;
}

std::shared_ptr<VerticalAxisRangeState> EventPlotWidget::getVerticalRangeState() const
{
    return _vertical_range_state;
}
