#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Rendering/EventPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"

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
      _vertical_axis_widget(nullptr)
{
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Create and add the vertical axis widget on the left
    _vertical_axis_widget = new VerticalAxisWidget(this);
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

    // Create and add the time axis widget below
    _axis_widget = new RelativeTimeAxisWidget(this);
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
