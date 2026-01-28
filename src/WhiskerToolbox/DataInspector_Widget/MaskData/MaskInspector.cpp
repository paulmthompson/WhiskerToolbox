#include "MaskInspector.hpp"

#include "Mask_Widget.hpp"

#include <QVBoxLayout>

MaskInspector::MaskInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

MaskInspector::~MaskInspector() {
    removeCallbacks();
}

void MaskInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_mask_widget) {
        _mask_widget->setActiveKey(key);
    }
}

void MaskInspector::removeCallbacks() {
    if (_mask_widget) {
        _mask_widget->removeCallbacks();
    }
}

void MaskInspector::updateView() {
    if (_mask_widget) {
        _mask_widget->updateTable();
    }
}

void MaskInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped Mask_Widget
    _mask_widget = std::make_unique<Mask_Widget>(dataManager(), this);
    _mask_widget->setGroupManager(groupManager());

    layout->addWidget(_mask_widget.get());
}

void MaskInspector::_connectSignals() {
    if (_mask_widget) {
        // Forward frame selection signal
        connect(_mask_widget.get(), &Mask_Widget::frameSelected,
                this, &MaskInspector::frameSelected);
    }
}
