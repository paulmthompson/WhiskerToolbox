#include "BaseDataView.hpp"

#include "DataInspector_Widget/DataInspectorState.hpp"

BaseDataView::BaseDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent)
    , _data_manager(std::move(data_manager)) {
}

BaseDataView::~BaseDataView() = default;

void BaseDataView::setState(std::shared_ptr<DataInspectorState> state) {
    _state = state;
}
