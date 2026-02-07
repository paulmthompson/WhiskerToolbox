#include "HeatmapWidget.hpp"

#include "Core/HeatmapState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Rendering/HeatmapOpenGLWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_HeatmapWidget.h"

HeatmapWidget::HeatmapWidget(std::shared_ptr<DataManager> data_manager,
                             QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::HeatmapWidget),
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

    // Create vertical axis state (Y-axis is trial/row-based, not serialized)
    _vertical_axis_state = std::make_unique<VerticalAxisState>(this);

    // Create combined vertical axis widget with range controls using factory
    auto vertical_axis_with_controls = createVerticalAxisWithRangeControls(
            _vertical_axis_state.get(), this, nullptr);
    _vertical_axis_widget = vertical_axis_with_controls.axis_widget;
    _vertical_range_controls = vertical_axis_with_controls.range_controls;
    _vertical_axis_state->setRange(0.0, 0.0);
    horizontal_layout->addWidget(_vertical_axis_widget);

    // Create and add the OpenGL widget
    _opengl_widget = new HeatmapOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);

    // Time axis widget and controls will be created in setState()
    _axis_widget = nullptr;
    _range_controls = nullptr;

    // Replace the main layout
    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &HeatmapOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });

    connect(_opengl_widget, &HeatmapOpenGLWidget::trialCountChanged,
            this, [this](size_t count) {
                _trial_count = count;
                if (_vertical_axis_widget) {
                    _vertical_axis_widget->update();
                }
            });
}

HeatmapWidget::~HeatmapWidget() {
    delete ui;
}

void HeatmapWidget::setState(std::shared_ptr<HeatmapState> state) {
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

void HeatmapWidget::createTimeAxisIfNeeded() {
    if (_axis_widget) {
        return;
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

void HeatmapWidget::wireTimeAxis() {
    if (!_axis_widget) {
        return;
    }

    _axis_widget->setAxisMapping(CorePlotting::relativeTimeAxis());

    _axis_widget->setViewStateGetter([this]() {
        if (!_state || !_opengl_widget) {
            return CorePlotting::ViewState{};
        }
        return toCoreViewState(
                _state->viewState(),
                _opengl_widget->width(),
                _opengl_widget->height());
    });
}

void HeatmapWidget::wireVerticalAxis() {
    if (!_vertical_axis_widget || !_state) {
        return;
    }

    // Dynamic axis mapping - updates when trial count changes
    connect(_opengl_widget, &HeatmapOpenGLWidget::trialCountChanged,
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

    // RangeGetter for axis tick rendering
    _vertical_axis_widget->setRangeGetter(
            [this]() { return computeVisibleTrialRange(); });

    // Bidirectional sync Flow A: AxisState (spinboxes) -> ViewState zoom/pan
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

void HeatmapWidget::connectViewChangeSignals() {
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

    connect(_state.get(), &HeatmapState::viewStateChanged,
            this, onViewChanged);

    connect(_opengl_widget, &HeatmapOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void HeatmapWidget::syncTimeAxisRange() {
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void HeatmapWidget::syncVerticalAxisRange() {
    if (!_vertical_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTrialRange();
    _vertical_axis_state->setRangeSilent(min, max);
}

std::pair<double, double> HeatmapWidget::computeVisibleTrialRange() const {
    if (_trial_count == 0 || !_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    auto const mapping = CorePlotting::trialIndexAxis(_trial_count);

    double const zoomed_half = 1.0 / vs.y_zoom;
    double const visible_bottom = -zoomed_half + vs.y_pan;
    double const visible_top = zoomed_half + vs.y_pan;

    double const a = mapping.worldToDomain(visible_bottom);
    double const b = mapping.worldToDomain(visible_top);
    return {std::min(a, b), std::max(a, b)};
}

std::pair<double, double> HeatmapWidget::computeVisibleTimeRange() const {
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
}

void HeatmapWidget::resizeEvent(QResizeEvent * event) {
    QWidget::resizeEvent(event);
    if (_axis_widget) {
        _axis_widget->update();
    }
}

HeatmapState * HeatmapWidget::state() {
    return _state.get();
}

RelativeTimeAxisRangeControls * HeatmapWidget::getRangeControls() const {
    return _range_controls;
}

VerticalAxisRangeControls * HeatmapWidget::getVerticalRangeControls() const {
    return _vertical_range_controls;
}

VerticalAxisState * HeatmapWidget::getVerticalAxisState() const {
    return _vertical_axis_state.get();
}
