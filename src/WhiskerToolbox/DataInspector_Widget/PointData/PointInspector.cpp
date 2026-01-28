#include "PointInspector.hpp"

#include "Point_Widget.hpp"

#include <QVBoxLayout>

PointInspector::PointInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

PointInspector::~PointInspector() {
    removeCallbacks();
}

void PointInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_point_widget) {
        _point_widget->setActiveKey(key);
    }
}

void PointInspector::removeCallbacks() {
    if (_point_widget) {
        _point_widget->removeCallbacks();
    }
}

void PointInspector::updateView() {
    if (_point_widget) {
        _point_widget->updateTable();
    }
}

void PointInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped Point_Widget
    _point_widget = std::make_unique<Point_Widget>(dataManager(), this);
    _point_widget->setGroupManager(groupManager());

    layout->addWidget(_point_widget.get());
}

void PointInspector::_connectSignals() {
    if (_point_widget) {
        // Forward frame selection signal
        connect(_point_widget.get(), &Point_Widget::frameSelected,
                this, &PointInspector::frameSelected);
    }
}
