#include "LineInspector.hpp"

#include "Line_Widget.hpp"

#include <QVBoxLayout>

LineInspector::LineInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

LineInspector::~LineInspector() {
    removeCallbacks();
}

void LineInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_line_widget) {
        _line_widget->setActiveKey(key);
    }
}

void LineInspector::removeCallbacks() {
    if (_line_widget) {
        _line_widget->removeCallbacks();
    }
}

void LineInspector::updateView() {
    if (_line_widget) {
        _line_widget->updateTable();
    }
}

void LineInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped Line_Widget
    _line_widget = std::make_unique<Line_Widget>(dataManager(), this);
    _line_widget->setGroupManager(groupManager());

    layout->addWidget(_line_widget.get());
}

void LineInspector::_connectSignals() {
    if (_line_widget) {
        // Forward frame selection signal
        connect(_line_widget.get(), &Line_Widget::frameSelected,
                this, &LineInspector::frameSelected);
    }
}
