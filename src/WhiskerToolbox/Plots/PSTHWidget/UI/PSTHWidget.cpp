#include "PSTHWidget.hpp"

#include "Core/PSTHState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/PSTHPlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_PSTHWidget.h"

PSTHWidget::PSTHWidget(std::shared_ptr<DataManager> data_manager,
                       QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::PSTHWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr)
{
    ui->setupUi(this);

    // Create horizontal layout for vertical axis + OpenGL widget
    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    // Vertical axis widget and controls will be created in setState()
    // when we have access to the PSTHState's VerticalAxisState
    // For now, add a placeholder (will be replaced in setState)
    _vertical_axis_widget = nullptr;
    _vertical_range_controls = nullptr;

    // Create and add the OpenGL widget
    _opengl_widget = new PSTHPlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);  // Stretch factor 1

    // Create vertical layout for horizontal layout + time axis
    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);  // Stretch factor 1

    // Time axis widget and controls will be created in setState()
    // when we have access to the PSTHState's RelativeTimeAxisState
    _axis_widget = nullptr;
    _range_controls = nullptr;

    // Replace the main layout
    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);

    // Forward signals from OpenGL widget
    connect(_opengl_widget, &PSTHPlotOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });
}

PSTHWidget::~PSTHWidget()
{
    delete ui;
}

void PSTHWidget::setState(std::shared_ptr<PSTHState> state)
{
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

    syncTimeAxisRange();
    syncVerticalAxisRange();
}

// ---------------------------------------------------------------------------
// setState decomposition
// ---------------------------------------------------------------------------

void PSTHWidget::createTimeAxisIfNeeded()
{
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

void PSTHWidget::wireTimeAxis()
{
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

void PSTHWidget::wireVerticalAxis()
{
    // Create the vertical axis widget if it doesn't exist yet
    if (!_vertical_axis_widget && _state) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            auto result = createVerticalAxisWithRangeControls(
                    vertical_axis_state, this, nullptr);
            _vertical_axis_widget = result.axis_widget;
            _vertical_range_controls = result.range_controls;

            // Insert into the horizontal layout (before the OpenGL widget)
            if (_vertical_axis_widget) {
                if (auto * vbox = qobject_cast<QVBoxLayout *>(layout())) {
                    if (vbox->count() > 0) {
                        auto * item = vbox->itemAt(0);
                        if (item && item->layout()) {
                            if (auto * hbox = qobject_cast<QHBoxLayout *>(item->layout())) {
                                hbox->insertWidget(0, _vertical_axis_widget);
                            }
                        }
                    }
                }
            }
        }
    }

    if (!_vertical_axis_widget || !_state) {
        return;
    }

    // The vertical axis is a simple linear scale (count axis).
    // The RangeGetter is already set by createVerticalAxisWithRangeControls
    // from the VerticalAxisState, which handles the y_min/y_max directly.
    // We use identity axis mapping (domain == world) for labels.
    _vertical_axis_widget->setAxisMapping(CorePlotting::identityAxis("Count", 0));

    // Bidirectional sync: VerticalAxisState → ViewState y_zoom/y_pan
    auto * vas = _state->verticalAxisState();
    if (vas) {
        connect(vas, &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state) return;
                    double const range = max_range - min_range;
                    if (range > 0.001) {
                        auto * vas_local = _state->verticalAxisState();
                        double const full_range = vas_local->getYMax() - vas_local->getYMin();
                        // Zoom = full_range / visible_range
                        _state->setYZoom(full_range / range);
                        _state->setPan(_state->viewState().x_pan,
                                       ((min_range + max_range) / 2.0) -
                                       ((vas_local->getYMin() + vas_local->getYMax()) / 2.0));
                    }
                });
    }
}

void PSTHWidget::connectViewChangeSignals()
{
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

    connect(_state.get(), &PSTHState::viewStateChanged,
            this, onViewChanged);

    connect(_opengl_widget, &PSTHPlotOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void PSTHWidget::syncTimeAxisRange()
{
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void PSTHWidget::syncVerticalAxisRange()
{
    if (!_state) {
        return;
    }
    auto * vas = _state->verticalAxisState();
    if (!vas) {
        return;
    }
    auto [min, max] = computeVisibleVerticalRange();
    vas->setRangeSilent(min, max);
}

// ---------------------------------------------------------------------------
// Visible-range helpers
// ---------------------------------------------------------------------------

std::pair<double, double> PSTHWidget::computeVisibleTimeRange() const
{
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
}

std::pair<double, double> PSTHWidget::computeVisibleVerticalRange() const
{
    if (!_state) {
        return {0.0, 100.0};
    }
    auto const & vs = _state->viewState();
    auto * vas = _state->verticalAxisState();
    double y_min = vas ? vas->getYMin() : 0.0;
    double y_max = vas ? vas->getYMax() : 100.0;
    double const y_range = y_max - y_min;
    double const y_center = (y_min + y_max) / 2.0;
    double const half = y_range / 2.0 / vs.y_zoom;
    return {y_center - half + vs.y_pan, y_center + half + vs.y_pan};
}

PSTHState * PSTHWidget::state()
{
    return _state.get();
}

RelativeTimeAxisRangeControls * PSTHWidget::getRangeControls() const
{
    return _range_controls;
}

VerticalAxisRangeControls * PSTHWidget::getVerticalRangeControls() const
{
    return _vertical_range_controls;
}

void PSTHWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    if (_axis_widget) {
        _axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}
