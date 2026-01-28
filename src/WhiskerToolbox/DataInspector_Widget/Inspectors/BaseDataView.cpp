#include "BaseDataView.hpp"

BaseDataView::BaseDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent)
    , _data_manager(std::move(data_manager)) {
}

BaseDataView::~BaseDataView() = default;
