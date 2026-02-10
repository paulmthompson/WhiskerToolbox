#include "RelativeTimeAxisWithRangeControls.hpp"

#include "Core/RelativeTimeAxisState.hpp"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>

#include <cmath>

RelativeTimeAxisRangeControls::RelativeTimeAxisRangeControls(
    RelativeTimeAxisState * state,
    QWidget * parent)
    : QWidget(parent),
      _state(state),
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
            this, &RelativeTimeAxisRangeControls::onMinRangeChanged);
    connect(_max_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RelativeTimeAxisRangeControls::onMaxRangeChanged);

    // Connect to state updates (programmatic changes)
    if (_state) {
        connect(_state, &RelativeTimeAxisState::rangeUpdated,
                this, &RelativeTimeAxisRangeControls::onStateRangeUpdated);
        // Also connect to rangeChanged to handle user changes from other sources
        connect(_state, &RelativeTimeAxisState::rangeChanged,
                this, &RelativeTimeAxisRangeControls::onStateRangeChanged);
    }

    // Initialize spinboxes
    updateSpinBoxes();
}

void RelativeTimeAxisRangeControls::onMinRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setMinRange(value);
}

void RelativeTimeAxisRangeControls::onMaxRangeChanged(double value)
{
    if (_updating_ui || !_state) {
        return;
    }

    _state->setMaxRange(value);
}

void RelativeTimeAxisRangeControls::onStateRangeUpdated(double min_range, double max_range)
{
    Q_UNUSED(min_range)
    Q_UNUSED(max_range)
    updateSpinBoxes();
}

void RelativeTimeAxisRangeControls::onStateRangeChanged(double min_range, double max_range)
{
    Q_UNUSED(min_range)
    Q_UNUSED(max_range)
    // Update spinboxes when range changes (could be from user input or programmatic)
    updateSpinBoxes();
}

void RelativeTimeAxisRangeControls::updateSpinBoxes()
{
    if (!_state) {
        return;
    }

    _updating_ui = true;

    // Update spinboxes if they don't already have the correct value
    if (std::abs(_min_spinbox->value() - _state->getMinRange()) > 0.01) {
        _min_spinbox->setValue(_state->getMinRange());
    }
    if (std::abs(_max_spinbox->value() - _state->getMaxRange()) > 0.01) {
        _max_spinbox->setValue(_state->getMaxRange());
    }

    _updating_ui = false;
}

void RelativeTimeAxisWithRangeControls::setViewStateGetter(RelativeTimeAxisWidget::ViewStateGetter getter)
{
    if (axis_widget) {
        axis_widget->setViewStateGetter(std::move(getter));
    }
}

void RelativeTimeAxisWithRangeControls::setRange(double min_range, double max_range)
{
    if (state) {
        state->setRange(min_range, max_range);
    }
}

void RelativeTimeAxisWithRangeControls::getRange(double & min_range, double & max_range) const
{
    if (state) {
        min_range = state->getMinRange();
        max_range = state->getMaxRange();
    } else {
        min_range = -500.0;
        max_range = 500.0;
    }
}

RelativeTimeAxisWithRangeControls createRelativeTimeAxisWithRangeControls(
    RelativeTimeAxisState * state,
    QWidget * axis_parent,
    QWidget * controls_parent)
{
    RelativeTimeAxisWithRangeControls result;
    result.state = state;

    // Create axis widget
    result.axis_widget = new RelativeTimeAxisWidget(axis_parent);

    // Create range controls widget
    result.range_controls = new RelativeTimeAxisRangeControls(state, controls_parent);

    return result;
}
