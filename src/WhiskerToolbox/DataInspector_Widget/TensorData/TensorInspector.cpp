#include "TensorInspector.hpp"

#include "Tensor_Widget.hpp"

#include <QVBoxLayout>

TensorInspector::TensorInspector(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : BaseInspector(std::move(data_manager), group_manager, parent) {
    _setupUi();
    _connectSignals();
}

TensorInspector::~TensorInspector() {
    removeCallbacks();
}

void TensorInspector::setActiveKey(std::string const & key) {
    _active_key = key;
    if (_tensor_widget) {
        _tensor_widget->setActiveKey(key);
    }
}

void TensorInspector::removeCallbacks() {
    // Tensor_Widget doesn't have removeCallbacks
    // (it doesn't use observers currently)
}

void TensorInspector::updateView() {
    if (_tensor_widget) {
        _tensor_widget->updateTable();
    }
}

void TensorInspector::_setupUi() {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create the wrapped Tensor_Widget
    _tensor_widget = std::make_unique<Tensor_Widget>(dataManager(), this);

    layout->addWidget(_tensor_widget.get());
}

void TensorInspector::_connectSignals() {
    // Tensor_Widget doesn't have frameSelected signal
    // No signal connections needed
}
