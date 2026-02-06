#include "RelativeTimeAxisWithRangeControls.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>

RelativeTimeAxisRangeState::RelativeTimeAxisRangeState(QObject * parent)
    : QObject(parent)
{
}

void RelativeTimeAxisRangeState::setRange(double min_range, double max_range)
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

void RelativeTimeAxisRangeState::setRangeFromUser(double min_range, double max_range)
{
    if (_min_range == min_range && _max_range == max_range) {
        return;
    }

    _min_range = min_range;
    _max_range = max_range;

    emit rangeChanged(_min_range, _max_range);
}

RelativeTimeAxisRangeControls::RelativeTimeAxisRangeControls(
    std::shared_ptr<RelativeTimeAxisRangeState> state,
    QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _min_combo(nullptr),
      _max_combo(nullptr),
      _updating_ui(false)
{
    auto * layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    // Min range label and combo
    auto * min_label = new QLabel("Min:", this);
    layout->addWidget(min_label);

    _min_combo = new QComboBox(this);
    _min_combo->setEditable(true);
    _min_combo->setMinimumWidth(100);
    layout->addWidget(_min_combo);

    // Separator
    auto * separator = new QLabel("to", this);
    layout->addWidget(separator);

    // Max range label and combo
    auto * max_label = new QLabel("Max:", this);
    layout->addWidget(max_label);

    _max_combo = new QComboBox(this);
    _max_combo->setEditable(true);
    _max_combo->setMinimumWidth(100);
    layout->addWidget(_max_combo);

    // Connect combo box signals
    connect(_min_combo, QOverload<QString const &>::of(&QComboBox::currentTextChanged),
            this, &RelativeTimeAxisRangeControls::onMinRangeChanged);
    connect(_max_combo, QOverload<QString const &>::of(&QComboBox::currentTextChanged),
            this, &RelativeTimeAxisRangeControls::onMaxRangeChanged);

    // Connect to state updates
    connect(_state.get(), &RelativeTimeAxisRangeState::rangeUpdated,
            this, &RelativeTimeAxisRangeControls::onStateRangeUpdated);

    // Initialize combo boxes
    updateComboBoxes();
}

void RelativeTimeAxisRangeControls::onMinRangeChanged()
{
    if (_updating_ui || !_state) {
        return;
    }

    bool ok = false;
    double min_value = _min_combo->currentText().toDouble(&ok);
    if (ok) {
        _state->setRangeFromUser(min_value, _state->maxRange());
    }
}

void RelativeTimeAxisRangeControls::onMaxRangeChanged()
{
    if (_updating_ui || !_state) {
        return;
    }

    bool ok = false;
    double max_value = _max_combo->currentText().toDouble(&ok);
    if (ok) {
        _state->setRangeFromUser(_state->minRange(), max_value);
    }
}

void RelativeTimeAxisRangeControls::onStateRangeUpdated(double min_range, double max_range)
{
    updateComboBoxes();
}

void RelativeTimeAxisRangeControls::updateComboBoxes()
{
    if (!_state) {
        return;
    }

    _updating_ui = true;

    QString min_text = QString::number(_state->minRange());
    QString max_text = QString::number(_state->maxRange());

    // Update combo boxes if they don't already have the correct value
    if (_min_combo->currentText() != min_text) {
        _min_combo->setCurrentText(min_text);
    }
    if (_max_combo->currentText() != max_text) {
        _max_combo->setCurrentText(max_text);
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
        min_range = state->minRange();
        max_range = state->maxRange();
    } else {
        min_range = -30000.0;
        max_range = 30000.0;
    }
}

RelativeTimeAxisWithRangeControls createRelativeTimeAxisWithRangeControls(
    QWidget * axis_parent,
    QWidget * controls_parent)
{
    RelativeTimeAxisWithRangeControls result;

    // Create shared state
    result.state = std::make_shared<RelativeTimeAxisRangeState>();

    // Create axis widget
    result.axis_widget = new RelativeTimeAxisWidget(axis_parent);

    // Create range controls widget
    result.range_controls = new RelativeTimeAxisRangeControls(result.state, controls_parent);

    return result;
}
