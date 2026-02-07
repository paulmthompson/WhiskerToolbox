#include "LinePlotWidget.hpp"

#include "Core/LinePlotState.hpp"
#include "Core/ViewStateAdapter.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "DataManager/DataManager.hpp"
#include "Rendering/LinePlotOpenGLWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_LinePlotWidget.h"

LinePlotWidget::LinePlotWidget(std::shared_ptr<DataManager> data_manager,
                               QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::LinePlotWidget),
      _opengl_widget(nullptr),
      _axis_widget(nullptr),
      _range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr)
{
    ui->setupUi(this);

    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    _opengl_widget = new LinePlotOpenGLWidget(this);
    _opengl_widget->setDataManager(_data_manager);
    horizontal_layout->addWidget(_opengl_widget, 1);

    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);

    _axis_widget = nullptr;
    _range_controls = nullptr;
    _vertical_axis_widget = nullptr;
    _vertical_range_controls = nullptr;

    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);

    connect(_opengl_widget, &LinePlotOpenGLWidget::plotDoubleClicked,
            this, [this](int64_t time_frame_index) {
                emit timePositionSelected(TimePosition(time_frame_index));
            });
}

LinePlotWidget::~LinePlotWidget()
{
    delete ui;
}

void LinePlotWidget::setState(std::shared_ptr<LinePlotState> state)
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

void LinePlotWidget::createTimeAxisIfNeeded()
{
    if (_axis_widget) {
        return;
    }

    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
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

void LinePlotWidget::wireTimeAxis()
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

void LinePlotWidget::wireVerticalAxis()
{
    if (!_vertical_axis_widget && _state) {
        auto * vertical_axis_state = _state->verticalAxisState();
        if (vertical_axis_state) {
            auto result = createVerticalAxisWithRangeControls(
                    vertical_axis_state, this, nullptr);
            _vertical_axis_widget = result.axis_widget;
            _vertical_range_controls = result.range_controls;

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

    _vertical_axis_widget->setAxisMapping(CorePlotting::identityAxis("Value", 0));

    auto * vas = _state->verticalAxisState();
    if (vas) {
        connect(vas, &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state) {
                        return;
                    }
                    double const range = max_range - min_range;
                    if (range > 0.001) {
                        auto * vas_local = _state->verticalAxisState();
                        double const full_range = vas_local->getYMax() - vas_local->getYMin();
                        _state->setYZoom(full_range / range);
                        _state->setPan(_state->viewState().x_pan,
                                       ((min_range + max_range) / 2.0) -
                                       ((vas_local->getYMin() + vas_local->getYMax()) / 2.0));
                    }
                });
    }
}

void LinePlotWidget::connectViewChangeSignals()
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

    connect(_state.get(), &LinePlotState::viewStateChanged,
            this, onViewChanged);

    connect(_opengl_widget, &LinePlotOpenGLWidget::viewBoundsChanged,
            this, onViewChanged);
}

void LinePlotWidget::syncTimeAxisRange()
{
    auto * time_axis_state = _state ? _state->relativeTimeAxisState() : nullptr;
    if (!time_axis_state) {
        return;
    }
    auto [min, max] = computeVisibleTimeRange();
    time_axis_state->setRangeSilent(min, max);
}

void LinePlotWidget::syncVerticalAxisRange()
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

std::pair<double, double> LinePlotWidget::computeVisibleTimeRange() const
{
    if (!_state) {
        return {0.0, 0.0};
    }
    auto const & vs = _state->viewState();
    double const half = (vs.x_max - vs.x_min) / 2.0 / vs.x_zoom;
    double const center = (vs.x_min + vs.x_max) / 2.0;
    return {center - half + vs.x_pan, center + half + vs.x_pan};
}

std::pair<double, double> LinePlotWidget::computeVisibleVerticalRange() const
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

LinePlotState * LinePlotWidget::state()
{
    return _state.get();
}

RelativeTimeAxisRangeControls * LinePlotWidget::getRangeControls() const
{
    return _range_controls;
}

VerticalAxisRangeControls * LinePlotWidget::getVerticalRangeControls() const
{
    return _vertical_range_controls;
}

void LinePlotWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    if (_axis_widget) {
        _axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}
