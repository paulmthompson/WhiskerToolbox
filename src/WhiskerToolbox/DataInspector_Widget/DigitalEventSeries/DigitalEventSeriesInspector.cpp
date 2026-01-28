#include "DigitalEventSeriesInspector.hpp"

#include "DigitalEventSeries_Widget.hpp"

#include <QVBoxLayout>

DigitalEventSeriesInspector::DigitalEventSeriesInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

DigitalEventSeriesInspector::~DigitalEventSeriesInspector() {
    removeCallbacks();
}

void DigitalEventSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_event_widget) {
        _event_widget->setActiveKey(key);
    }
}

void DigitalEventSeriesInspector::removeCallbacks() {
    if (_event_widget) {
        _event_widget->removeCallbacks();
    }
}

void DigitalEventSeriesInspector::updateView() {
    // DigitalEventSeries_Widget auto-updates through callbacks
    // No explicit updateTable method
}

void DigitalEventSeriesInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped DigitalEventSeries_Widget
    _event_widget = std::make_unique<DigitalEventSeries_Widget>(dataManager(), this);

    layout->addWidget(_event_widget.get());
}

void DigitalEventSeriesInspector::_connectSignals() {
    if (_event_widget) {
        // Forward frame selection signal
        connect(_event_widget.get(), &DigitalEventSeries_Widget::frameSelected,
                this, &DigitalEventSeriesInspector::frameSelected);
    }
}
