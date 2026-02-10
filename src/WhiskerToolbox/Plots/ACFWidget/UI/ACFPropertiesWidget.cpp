#include "ACFPropertiesWidget.hpp"

#include "Core/ACFState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Collapsible_Widget/Section.hpp"
#include "UI/ACFWidget.hpp"

#include "ui_ACFPropertiesWidget.h"

#include <algorithm>

ACFPropertiesWidget::ACFPropertiesWidget(std::shared_ptr<ACFState> state,
                                          std::shared_ptr<DataManager> data_manager,
                                          QWidget * parent)
    : QWidget(parent),
      ui(new Ui::ACFPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr),
      _horizontal_range_controls(nullptr),
      _horizontal_range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _dm_observer_id(-1)
{
    ui->setupUi(this);

    // Connect combo box selection change
    connect(ui->event_key_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ACFPropertiesWidget::_onEventKeyComboChanged);

    // Populate combo box
    _populateEventKeyComboBox();

    // Set up DataManager observer to refresh combo box when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateEventKeyComboBox();
        });
    }

    // Connect state signals to update UI
    if (_state) {
        connect(_state.get(), &ACFState::eventKeyChanged,
                this, &ACFPropertiesWidget::_updateUIFromState);

        _updateUIFromState();
    }
}

void ACFPropertiesWidget::setPlotWidget(ACFWidget * plot_widget)
{
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * horizontal_axis_state = _state->horizontalAxisState();
    if (horizontal_axis_state) {
        _horizontal_range_controls_section = new Section(this, "X-Axis (Lag) Range Controls");
        _horizontal_range_controls = new HorizontalAxisRangeControls(horizontal_axis_state, _horizontal_range_controls_section);
        _horizontal_range_controls_section->autoSetContentLayout();
        ui->main_layout->insertWidget(0, _horizontal_range_controls_section);
    }

    auto * vertical_axis_state = _state->verticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis (Value) Range Controls");
        _vertical_range_controls = new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _horizontal_range_controls_section
                ? ui->main_layout->indexOf(_horizontal_range_controls_section) + 1
                : 0;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

ACFPropertiesWidget::~ACFPropertiesWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void ACFPropertiesWidget::_populateEventKeyComboBox()
{
    ui->event_key_combo->blockSignals(true);

    // Save current selection before clearing
    QString current_selection;
    if (_state) {
        current_selection = _state->getEventKey();
    }

    ui->event_key_combo->clear();

    if (!_data_manager) {
        ui->event_key_combo->blockSignals(false);
        return;
    }

    // Get all DigitalEventSeries keys only
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();

    if (event_keys.empty()) {
        ui->event_key_combo->blockSignals(false);
        return;
    }

    // Sort keys before adding to combo box
    std::sort(event_keys.begin(), event_keys.end());

    // Add DigitalEventSeries keys
    int selected_index = -1;
    for (auto const & key: event_keys) {
        QString key_str = QString::fromStdString(key);
        ui->event_key_combo->addItem(key_str, key_str);
        if (key_str == current_selection) {
            selected_index = ui->event_key_combo->count() - 1;
        }
    }

    // Restore selection if it still exists
    if (selected_index >= 0) {
        ui->event_key_combo->setCurrentIndex(selected_index);
    }

    ui->event_key_combo->blockSignals(false);
}

void ACFPropertiesWidget::_onEventKeyComboChanged(int index)
{
    if (!_state || index < 0) {
        return;
    }

    QString event_key = ui->event_key_combo->itemData(index).toString();
    if (!event_key.isEmpty()) {
        _state->setEventKey(event_key);
    }
}

void ACFPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    QString current_event_key = _state->getEventKey();
    if (current_event_key.isEmpty()) {
        ui->event_key_combo->setCurrentIndex(-1);
        return;
    }

    // Find the index of the current event key in the combo box
    int index = ui->event_key_combo->findData(current_event_key);
    if (index >= 0) {
        ui->event_key_combo->blockSignals(true);
        ui->event_key_combo->setCurrentIndex(index);
        ui->event_key_combo->blockSignals(false);
    }
}
