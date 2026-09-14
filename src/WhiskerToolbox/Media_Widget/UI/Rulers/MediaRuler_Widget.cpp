#include "MediaRuler_Widget.hpp"

#include "Core/MediaWidgetState.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>

MediaRuler_Widget::MediaRuler_Widget(QWidget * parent)
    : QWidget(parent) {
    _buildUi();
}

void MediaRuler_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    if (_state) {
        connect(_state, &MediaWidgetState::rulerPrefsChanged, this, &MediaRuler_Widget::_syncFromState);
        _syncFromState();
    }
}

void MediaRuler_Widget::_buildUi() {
    auto * layout = new QFormLayout(this);

    _enabled_checkbox = new QCheckBox(QStringLiteral("Show rulers"), this);
    _enabled_checkbox->setChecked(true);
    layout->addRow(_enabled_checkbox);

    _tick_mode_combo = new QComboBox(this);
    _tick_mode_combo->addItem(QStringLiteral("Auto"), static_cast<int>(RulerTickMode::Auto));
    _tick_mode_combo->addItem(QStringLiteral("Fixed interval"), static_cast<int>(RulerTickMode::Fixed));
    layout->addRow(QStringLiteral("Tick mode:"), _tick_mode_combo);

    _fixed_interval_spin = new QSpinBox(this);
    _fixed_interval_spin->setRange(1, 10000);
    _fixed_interval_spin->setSuffix(QStringLiteral(" px"));
    _fixed_interval_spin->setValue(100);
    layout->addRow(QStringLiteral("Fixed interval:"), _fixed_interval_spin);

    _target_tick_count_spin = new QSpinBox(this);
    _target_tick_count_spin->setRange(3, 20);
    _target_tick_count_spin->setValue(7);
    layout->addRow(QStringLiteral("Target ticks:"), _target_tick_count_spin);

    _minor_ticks_checkbox = new QCheckBox(QStringLiteral("Show minor ticks"), this);
    _minor_ticks_checkbox->setChecked(true);
    layout->addRow(_minor_ticks_checkbox);

    _negative_color_button = new QPushButton(this);
    _negative_color_button->setText(QStringLiteral("Choose color"));
    layout->addRow(QStringLiteral("Negative color:"), _negative_color_button);

    connect(_enabled_checkbox, &QCheckBox::toggled, this, &MediaRuler_Widget::_onPrefsChanged);
    connect(_tick_mode_combo, &QComboBox::currentIndexChanged, this, [this](int) {
        _updateModeControls();
        _onPrefsChanged();
    });
    connect(_fixed_interval_spin, &QSpinBox::valueChanged, this, &MediaRuler_Widget::_onPrefsChanged);
    connect(_target_tick_count_spin, &QSpinBox::valueChanged, this, &MediaRuler_Widget::_onPrefsChanged);
    connect(_minor_ticks_checkbox, &QCheckBox::toggled, this, &MediaRuler_Widget::_onPrefsChanged);
    connect(_negative_color_button, &QPushButton::clicked, this, &MediaRuler_Widget::_onNegativeColorClicked);

    _updateModeControls();
}

void MediaRuler_Widget::_updateModeControls() {
    bool const fixed_mode = _tick_mode_combo->currentData().toInt() == static_cast<int>(RulerTickMode::Fixed);
    _fixed_interval_spin->setEnabled(fixed_mode);
    _target_tick_count_spin->setEnabled(!fixed_mode);
}

RulerPrefs MediaRuler_Widget::_readPrefsFromUi() const {
    RulerPrefs prefs;
    prefs.enabled = _enabled_checkbox->isChecked();
    prefs.tick_mode = static_cast<RulerTickMode>(_tick_mode_combo->currentData().toInt());
    prefs.fixed_interval_px = _fixed_interval_spin->value();
    prefs.target_tick_count = _target_tick_count_spin->value();
    prefs.show_minor_ticks = _minor_ticks_checkbox->isChecked();
    return prefs;
}

void MediaRuler_Widget::_onPrefsChanged() {
    if (_updating_from_state || !_state) {
        return;
    }

    RulerPrefs prefs = _readPrefsFromUi();
    prefs.negative_color = _state->rulerPrefs().negative_color;
    _state->setRulerPrefs(prefs);
}

void MediaRuler_Widget::_onNegativeColorClicked() {
    if (!_state) {
        return;
    }

    QColor const current = QColor(QString::fromStdString(_state->rulerPrefs().negative_color));
    QColor const chosen = QColorDialog::getColor(current, this, QStringLiteral("Negative label color"));
    if (!chosen.isValid()) {
        return;
    }

    RulerPrefs prefs = _readPrefsFromUi();
    prefs.negative_color = chosen.name(QColor::HexRgb).toStdString();
    _state->setRulerPrefs(prefs);
}

void MediaRuler_Widget::_syncFromState() {
    if (!_state) {
        return;
    }

    _updating_from_state = true;

    RulerPrefs const & prefs = _state->rulerPrefs();
    _enabled_checkbox->setChecked(prefs.enabled);

    int const mode_index = _tick_mode_combo->findData(static_cast<int>(prefs.tick_mode));
    if (mode_index >= 0) {
        _tick_mode_combo->setCurrentIndex(mode_index);
    }

    _fixed_interval_spin->setValue(prefs.fixed_interval_px);
    _target_tick_count_spin->setValue(prefs.target_tick_count);
    _minor_ticks_checkbox->setChecked(prefs.show_minor_ticks);

    QColor const negative_color(QString::fromStdString(prefs.negative_color));
    _negative_color_button->setStyleSheet(
            QStringLiteral("background-color: %1;").arg(negative_color.name()));

    _updateModeControls();
    _updating_from_state = false;
}
