#include "OnionSkinViewWidget.hpp"

#include "Core/OnionSkinViewState.hpp"
#include "CorePlotting/CoordinateTransform/AxisMapping.hpp"
#include "DataManager/DataManager.hpp"
#include "Plots/Common/HorizontalAxisWidget/Core/HorizontalAxisState.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWidget.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/Core/VerticalAxisState.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWidget.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Rendering/OnionSkinViewOpenGLWidget.hpp"

#include <QHBoxLayout>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui_OnionSkinViewWidget.h"

OnionSkinViewWidget::OnionSkinViewWidget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      _data_manager(data_manager),
      ui(new Ui::OnionSkinViewWidget),
      _opengl_widget(nullptr),
      _horizontal_axis_widget(nullptr),
      _horizontal_range_controls(nullptr),
      _vertical_axis_widget(nullptr),
      _vertical_range_controls(nullptr)
{
    ui->setupUi(this);

    auto * horizontal_layout = new QHBoxLayout();
    horizontal_layout->setSpacing(0);
    horizontal_layout->setContentsMargins(0, 0, 0, 0);

    _opengl_widget = new OnionSkinViewOpenGLWidget(this);
    horizontal_layout->addWidget(_opengl_widget, 1);

    auto * vertical_layout = new QVBoxLayout();
    vertical_layout->setSpacing(0);
    vertical_layout->setContentsMargins(0, 0, 0, 0);
    vertical_layout->addLayout(horizontal_layout, 1);

    QLayout * old_layout = layout();
    if (old_layout) {
        delete old_layout;
    }
    setLayout(vertical_layout);
}

OnionSkinViewWidget::~OnionSkinViewWidget()
{
    delete ui;
}

void OnionSkinViewWidget::setState(std::shared_ptr<OnionSkinViewState> state)
{
    _state = state;
    if (_opengl_widget) {
        _opengl_widget->setState(_state);
        _opengl_widget->setDataManager(_data_manager);
    }
    if (!_state) {
        return;
    }

    createVerticalAxisIfNeeded();
    createHorizontalAxisIfNeeded();
    wireHorizontalAxis();
    wireVerticalAxis();
    connectViewChangeSignals();
    syncHorizontalAxisRange();
    syncVerticalAxisRange();
}

void OnionSkinViewWidget::createVerticalAxisIfNeeded()
{
    if (_vertical_axis_widget) {
        return;
    }
    auto * vertical_axis_state = _state ? _state->verticalAxisState() : nullptr;
    if (!vertical_axis_state) {
        return;
    }
    auto result = createVerticalAxisWithRangeControls(vertical_axis_state, this, nullptr);
    _vertical_axis_widget = result.axis_widget;
    _vertical_range_controls = result.range_controls;
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

void OnionSkinViewWidget::createHorizontalAxisIfNeeded()
{
    if (_horizontal_axis_widget) {
        return;
    }
    auto * horizontal_axis_state = _state ? _state->horizontalAxisState() : nullptr;
    if (!horizontal_axis_state) {
        return;
    }
    auto result = createHorizontalAxisWithRangeControls(horizontal_axis_state, this, nullptr);
    _horizontal_axis_widget = result.axis_widget;
    _horizontal_range_controls = result.range_controls;
    if (auto * vbox = qobject_cast<QVBoxLayout *>(layout())) {
        vbox->addWidget(_horizontal_axis_widget);
    }
}

void OnionSkinViewWidget::wireHorizontalAxis()
{
    if (!_horizontal_axis_widget) {
        return;
    }
    _horizontal_axis_widget->setAxisMapping(CorePlotting::identityAxis("X", 0));
    _horizontal_axis_widget->setRangeGetter([this]() {
        if (!_state) {
            return std::make_pair(0.0, 100.0);
        }
        auto [min, max] = computeVisibleXRange();
        return std::make_pair(min, max);
    });

    auto * has = _state->horizontalAxisState();
    if (has) {
        connect(has, &HorizontalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state) {
                        return;
                    }
                    double const range = max_range - min_range;
                    if (range > 0.001) {
                        auto const & vs = _state->viewState();
                        double const full_range = vs.x_max - vs.x_min;
                        _state->setXZoom(full_range / range);
                        _state->setPan(((min_range + max_range) / 2.0) -
                                           ((vs.x_min + vs.x_max) / 2.0),
                                       vs.y_pan);
                    }
                });
    }
}

void OnionSkinViewWidget::wireVerticalAxis()
{
    if (!_vertical_axis_widget || !_state) {
        return;
    }
    _vertical_axis_widget->setAxisMapping(CorePlotting::identityAxis("Y", 0));
    _vertical_axis_widget->setRangeGetter([this]() {
        if (!_state) {
            return std::make_pair(0.0, 100.0);
        }
        auto [min, max] = computeVisibleYRange();
        return std::make_pair(min, max);
    });

    auto * vas = _state->verticalAxisState();
    if (vas) {
        connect(vas, &VerticalAxisState::rangeChanged,
                this, [this](double min_range, double max_range) {
                    if (!_state) {
                        return;
                    }
                    double const range = max_range - min_range;
                    if (range > 0.001) {
                        auto const & vs = _state->viewState();
                        double const full_range = vs.y_max - vs.y_min;
                        _state->setYZoom(full_range / range);
                        _state->setPan(vs.x_pan,
                                       ((min_range + max_range) / 2.0) -
                                           ((vs.y_min + vs.y_max) / 2.0));
                    }
                });
    }
}

void OnionSkinViewWidget::connectViewChangeSignals()
{
    auto onViewChanged = [this]() {
        if (_horizontal_axis_widget) {
            _horizontal_axis_widget->update();
        }
        if (_vertical_axis_widget) {
            _vertical_axis_widget->update();
        }
        syncHorizontalAxisRange();
        syncVerticalAxisRange();
    };
    connect(_state.get(), &OnionSkinViewState::viewStateChanged, this, onViewChanged);
    connect(_opengl_widget, &OnionSkinViewOpenGLWidget::viewBoundsChanged, this,
            onViewChanged);
}

void OnionSkinViewWidget::syncHorizontalAxisRange()
{
    auto * has = _state ? _state->horizontalAxisState() : nullptr;
    if (!has) {
        return;
    }
    auto [min, max] = computeVisibleXRange();
    has->setRangeSilent(min, max);
}

void OnionSkinViewWidget::syncVerticalAxisRange()
{
    auto * vas = _state ? _state->verticalAxisState() : nullptr;
    if (!vas) {
        return;
    }
    auto [min, max] = computeVisibleYRange();
    vas->setRangeSilent(min, max);
}

std::pair<double, double> OnionSkinViewWidget::computeVisibleXRange() const
{
    if (!_state) {
        return {0.0, 100.0};
    }
    auto const & vs = _state->viewState();
    double const x_range = vs.x_max - vs.x_min;
    double const x_center = (vs.x_min + vs.x_max) / 2.0;
    double const half = x_range / 2.0 / vs.x_zoom;
    return {x_center - half + vs.x_pan, x_center + half + vs.x_pan};
}

std::pair<double, double> OnionSkinViewWidget::computeVisibleYRange() const
{
    if (!_state) {
        return {0.0, 100.0};
    }
    // Y data bounds are in view state (kept in sync with vertical axis via setYBounds)
    auto const & vs = _state->viewState();
    double const y_range = vs.y_max - vs.y_min;
    double const y_center = (vs.y_min + vs.y_max) / 2.0;
    double const half = y_range / 2.0 / vs.y_zoom;
    return {y_center - half + vs.y_pan, y_center + half + vs.y_pan};
}

void OnionSkinViewWidget::_onTimeChanged(TimePosition position)
{
    if (_opengl_widget) {
        _opengl_widget->setCurrentTime(position.index.getValue());
    }
}

OnionSkinViewState * OnionSkinViewWidget::state()
{
    return _state.get();
}

HorizontalAxisRangeControls * OnionSkinViewWidget::getHorizontalRangeControls() const
{
    return _horizontal_range_controls;
}

VerticalAxisRangeControls * OnionSkinViewWidget::getVerticalRangeControls() const
{
    return _vertical_range_controls;
}

void OnionSkinViewWidget::resizeEvent(QResizeEvent * event)
{
    QWidget::resizeEvent(event);
    if (_horizontal_axis_widget) {
        _horizontal_axis_widget->update();
    }
    if (_vertical_axis_widget) {
        _vertical_axis_widget->update();
    }
}
