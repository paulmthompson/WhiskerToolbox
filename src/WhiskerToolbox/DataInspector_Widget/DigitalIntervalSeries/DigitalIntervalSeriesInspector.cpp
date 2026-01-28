#include "DigitalIntervalSeriesInspector.hpp"

#include "DigitalIntervalSeries_Widget.hpp"

#include <QVBoxLayout>

DigitalIntervalSeriesInspector::DigitalIntervalSeriesInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

DigitalIntervalSeriesInspector::~DigitalIntervalSeriesInspector() {
    removeCallbacks();
}

void DigitalIntervalSeriesInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_interval_widget) {
        _interval_widget->setActiveKey(key);
    }
}

void DigitalIntervalSeriesInspector::removeCallbacks() {
    if (_interval_widget) {
        _interval_widget->removeCallbacks();
    }
}

void DigitalIntervalSeriesInspector::updateView() {
    // DigitalIntervalSeries_Widget auto-updates through callbacks
    // No explicit updateTable method
}

void DigitalIntervalSeriesInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped DigitalIntervalSeries_Widget
    _interval_widget = std::make_unique<DigitalIntervalSeries_Widget>(dataManager(), this);

    layout->addWidget(_interval_widget.get());
}

void DigitalIntervalSeriesInspector::_connectSignals() {
    if (_interval_widget) {
        // Forward frame selection signal
        connect(_interval_widget.get(), &DigitalIntervalSeries_Widget::frameSelected,
                this, &DigitalIntervalSeriesInspector::frameSelected);
    }
}
