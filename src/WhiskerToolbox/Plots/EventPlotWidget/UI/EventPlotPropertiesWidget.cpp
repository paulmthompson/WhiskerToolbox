#include "EventPlotPropertiesWidget.hpp"

#include "Core/EventPlotState.hpp"
#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include "ui_EventPlotPropertiesWidget.h"

#include <algorithm>

EventPlotPropertiesWidget::EventPlotPropertiesWidget(std::shared_ptr<EventPlotState> state,
                                                     std::shared_ptr<DataManager> data_manager,
                                                     QWidget * parent)
    : QWidget(parent),
      ui(new Ui::EventPlotPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _dm_observer_id(-1)
{
    ui->setupUi(this);
    
    // Populate combo box initially
    _populateEventSeriesComboBox();
    
    // Set up callback to refresh combo box when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populateEventSeriesComboBox();
        });
    }
}

EventPlotPropertiesWidget::~EventPlotPropertiesWidget()
{
    // Remove DataManager observer callback
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void EventPlotPropertiesWidget::_populateEventSeriesComboBox()
{
    ui->event_series_combo->clear();
    
    if (!_data_manager) {
        return;
    }

    // Get all DigitalEventSeries keys
    auto event_keys = _data_manager->getKeys<DigitalEventSeries>();
    
    // Get all DigitalIntervalSeries keys
    auto interval_keys = _data_manager->getKeys<DigitalIntervalSeries>();
    
    if (event_keys.empty() && interval_keys.empty()) {
        ui->event_series_combo->addItem("No event data available");
        ui->event_series_combo->setEnabled(false);
        return;
    }

    ui->event_series_combo->setEnabled(true);
    
    // Sort keys before adding to combo box
    std::sort(event_keys.begin(), event_keys.end());
    std::sort(interval_keys.begin(), interval_keys.end());
    
    // Add DigitalEventSeries keys
    for (auto const & key : event_keys) {
        ui->event_series_combo->addItem(QString::fromStdString(key));
    }
    
    // Add DigitalIntervalSeries keys
    for (auto const & key : interval_keys) {
        ui->event_series_combo->addItem(QString::fromStdString(key));
    }
}
