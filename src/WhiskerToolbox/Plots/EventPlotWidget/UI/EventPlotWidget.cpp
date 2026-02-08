#include "EventPlotWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "CorePlotting/CoordinateTransform/ViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
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

    if (_opengl_widget) {
        _opengl_widget->setState(_state);
    }

    if (!_state) {
        return;
    }

    createTimeAxisIfNeeded();
    wireTimeAxis();
    wireVerticalAxis();
    connectViewChangeSignals();

    // Initialize axis ranges from current view state
    syncTimeAxisRange();
    syncVerticalAxisRange();
}

// ---------------------------------------------------------------------------
// setState decomposition — each method handles one axis or concern
// ---------------------------------------------------------------------------

void EventPlotWidget::createTimeAxisIfNeeded() {
    if (_axis_widget) {
        return;// Already created
    }

    auto * time_axis_state = _state->relativeTimeAxisState();
    if (!time_axis_state) {
        return;
    }

    auto result = createRelativeTimeAxisWithRangeControls(
            time_axis_state, this, nullptr);
    _axis_widget = result.axis_widget;
    _range_controls = result.range_controls;

    if (auto * vbox = qobject_cast<QVBoxLayout *>(layout())) {
        vbox->addWidget(_axis_widget);
    }
}

void EventPlotWidget::wireTimeAxis() {
    if (!_axis_widget) {
        return;
    }

    _axis_widget->setAxisMapping(CorePlotting::relativeTimeAxis());

    _axis_widget->setViewStateGetter([this]() {
        if (!_state || !_opengl_widget) {
            return CorePlotting::ViewState{};
        }
        return CorePlotting::toRuntimeViewState(
                _state->viewState(),
                _opengl_widget->width(),
                _opengl_widget->height());
    });
}

void EventPlotWidget::wireVerticalAxis() {
    if (!_vertical_axis_widget || !_state) {
        return;
    }

    // Dynamic axis mapping — updates when trial count changes
    connect(_opengl_widget, &EventPlotOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                if (count > 0) {
                    _vertical_axis_widget->setAxisMapping(
                            CorePlotting::trialIndexAxis(count));
                }
            });

    if (_trial_count > 0) {
        _vertical_axis_widget->setAxisMapping(
                CorePlotting::trialIndexAxis(_trial_count));
    }

    // RangeGetter for axis tick rendering — delegates to shared computation
    _vertical_axis_widget->setRangeGetter(
            [this]() { return computeVisibleTrialRange(); });

    // Bidirectional sync Flow A: AxisState (spinboxes) → ViewState zoom/pan
    if (_vertical_axis_state) {
        connect(_vertical_axis_state.get(), &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state || _trial_count == 0) {
                        return;
                    }
                    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);
                    double const world_y_min = mapping.domainToWorld(min_range);
                    double const world_y_max = mapping.domainToWorld(max_range);
                    double const world_range = world_y_max - world_y_min;
                    if (world_range > 0.001) {
                        _state->setYZoom(2.0 / world_range);
                        _state->setPan(_state->viewState().x_pan,
                                       (world_y_min + world_y_max) / 2.0);
                    }
                });
    }
}

void EventPlotWidget::connectViewChangeSignals() {
    // Single handler consolidates all view-change reactions:
    //   • repaint both axis widgets
    //   • sync time & vertical range controls (silent — avoids feedback loops)
    auto onViewChanged = [this]() {
        if (_axis_widget) {
            _axis_widget->update();
        }
        if (_vertical_axis_widget) {
            _vertical_axis_widget->update();
        }
        syncTimeAxisRange();
        syncVerticalAxisRange();
    };

    // ViewState changes come from EventPlotState (zoom/pan/bounds setters)
    connect(_state.get(), &EventPlotState::viewStateChanged,
            this, onViewChanged);

    // viewBoundsChanged comes from the OpenGL widget (resize, interaction)
    connect(_opengl_widget, &EventPlotOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void EventPlotWidget::syncTimeAxisRange() {
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void EventPlotWidget::syncVerticalAxisRange() {
    if (!_vertical_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTrialRange();
    _vertical_axis_state->setRangeSilent(min, max);
}

// ---------------------------------------------------------------------------
// Visible-range helpers — single source of truth for zoom/pan → domain math
// ---------------------------------------------------------------------------

std::pair<double, double> EventPlotWidget::computeVisibleTrialRange() const {
    if (_trial_count == 0 || !_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

    // Y world coordinates span [-1, 1]; zoom/pan shift the visible window
    double const zoomed_half = 1.0 / vs.y_zoom;
    double const visible_bottom = -zoomed_half + vs.y_pan;
    double const visible_top = zoomed_half + vs.y_pan;

    double const a = mapping.worldToDomain(visible_bottom);
    double const b = mapping.worldToDomain(visible_top);
    return {std::min(a, b), std::max(a, b)};
}

std::pair<double, double> EventPlotWidget::computeVisibleTimeRange() const {
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
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
