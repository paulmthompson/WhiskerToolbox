#include "VerticalAxisWithRangeControls.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

VerticalAxisRangeState::VerticalAxisRangeState(QObject * parent)
    : QObject(parent)
{
}

void VerticalAxisRangeState::setRange(double min_range, double max_range)
{
    if (_min_range == min_range && _max_range == max_range) {
        return;
    }

    _updating = true;
    _min_range = min_range;
    _max_range = max_range;
    _updating = false;

    emit rangeUpdated(_min_range, _max_range);
}

void VerticalAxisRangeState::setRangeFromUser(double min_range, double max_range)
{
    if (_min_range == min_range && _max_range == max_range) {
        return;
    }

    _min_range = min_range;
    _max_range = max_range;

    emit rangeChanged(_min_range, _max_range);
}

VerticalAxisRangeControls::VerticalAxisRangeControls(
    std::shared_ptr<VerticalAxisRangeState> state,
    QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _min_spinbox(nullptr),
      _max_spinbox(nullptr),
      _updating_ui(false)
{
    auto * layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // Min range label and spinbox
    auto * min_label = new QLabel("Min:", this);
    layout->addWidget(min_label);

    _min_spinbox = new QDoubleSpinBox(this);
    _min_spinbox->setMinimum(-1000000.0);
    _min_spinbox->setMaximum(1000000.0);
    _min_spinbox->setDecimals(1);
    _min_spinbox->setMinimumWidth(100);
    layout->addWidget(_min_spinbox);

    // Separator
    auto * separator = new QLabel("to", this);
    layout->addWidget(separator);

    // Max range label and spinbox
    auto * max_label = new QLabel("Max:", this);
    layout->addWidget(max_label);

    _max_spinbox = new QDoubleSpinBox(this);
    _max_spinbox->setMinimum(-1000000.0);
    _max_spinbox->setMaximum(1000000.0);
    _max_spinbox->setDecimals(1);
    _max_spinbox->setMinimumWidth(100);
    layout->addWidget(_max_spinbox);

    // Connect spinbox signals
    connect(_min_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VerticalAxisRangeControls::onMinRangeChanged);
    connect(_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &VerticalAxisRangeControls::onMaxRangeChanged);

    // Connect to state updates
    connect(_state.get(), &VerticalAxisRangeState::rangeUpdated,
            this, &VerticalAxisRangeControls::onStateRangeUpdated);

    // Initialize spinboxes
    updateSpinBoxes();
}

void VerticalAxisRangeControls::onMinRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setRangeFromUser(value, _state->maxRange());
}

void VerticalAxisRangeControls::onMaxRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setRangeFromUser(_state->minRange(), value);
}

void VerticalAxisRangeControls::onStateRangeUpdated(double min_range, double max_range)
{
    updateSpinBoxes();
}

void VerticalAxisRangeControls::updateSpinBoxes()
{
    if (!_state) {
        return;
    }

    _updating_ui = true;

    // Update spinboxes if they don't already have the correct value
    if (std::abs(_min_spinbox->value() - _state->minRange()) > 0.01) {
        _min_spinbox->setValue(_state->minRange());
    }
    if (std::abs(_max_spinbox->value() - _state->maxRange()) > 0.01) {
        _max_spinbox->setValue(_state->maxRange());
    }

    _updating_ui = false;
}

void VerticalAxisWithRangeControls::setRangeGetter(VerticalAxisWidget::RangeGetter getter)
{
    if (axis_widget) {
        axis_widget->setRangeGetter(std::move(getter));
    }
}

void VerticalAxisWithRangeControls::setRange(double min_range, double max_range)
{
    if (state) {
        state->setRange(min_range, max_range);
    }
}

void VerticalAxisWithRangeControls::getRange(double & min_range, double & max_range) const
{
    if (state) {
        min_range = state->minRange();
        max_range = state->maxRange();
    } else {
        min_range = 0.0;
        max_range = 100.0;
    }
}

VerticalAxisWithRangeControls createVerticalAxisWithRangeControls(
    QWidget * axis_parent,
    QWidget * controls_parent)
{
    VerticalAxisWithRangeControls result;

    // Create shared state
    result.state = std::make_shared<VerticalAxisRangeState>();

    // Create axis widget
    result.axis_widget = new VerticalAxisWidget(axis_parent);

    // Create range controls widget
    result.range_controls = new VerticalAxisRangeControls(result.state, controls_parent);

    return result;
}