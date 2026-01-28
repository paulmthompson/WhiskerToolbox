#include "AnalogTimeSeriesInspector.hpp"

#include "AnalogTimeSeries_Widget.hpp"

#include <QVBoxLayout>

AnalogTimeSeriesInspector::AnalogTimeSeriesInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

AnalogTimeSeriesInspector::~AnalogTimeSeriesInspector() {
    removeCallbacks();
}

void AnalogTimeSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_analog_widget) {
        _analog_widget->setActiveKey(key);
    }
}

void AnalogTimeSeriesInspector::removeCallbacks() {
    // AnalogTimeSeries_Widget doesn't have removeCallbacks
    // (it doesn't use observers currently)
}

void AnalogTimeSeriesInspector::updateView() {
    // AnalogTimeSeries_Widget auto-updates through setActiveKey
    // No explicit updateTable method
}

void AnalogTimeSeriesInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped AnalogTimeSeries_Widget
    _analog_widget = std::make_unique<AnalogTimeSeries_Widget>(dataManager(), this);

    layout->addWidget(_analog_widget.get());
}

void AnalogTimeSeriesInspector::_connectSignals() {
    // AnalogTimeSeries_Widget doesn't have frameSelected signal
    // No signal connections needed
}
