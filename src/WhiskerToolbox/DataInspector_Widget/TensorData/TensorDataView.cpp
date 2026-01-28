#include "TensorDataView.hpp"

#include "TensorTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/Tensor_Data.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

TensorDataView::TensorDataView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new TensorTableModel(this)) {
    _setupUi();
    _connectSignals();
}

TensorDataView::~TensorDataView() {
    removeCallbacks();
}

void TensorDataView::setActiveKey(std::string const & key) {
    _active_key = key;

    auto tensor_data = dataManager()->getData<TensorData>(_active_key);
    if (tensor_data) {
        _table_model->setTensorData(tensor_data.get());
    } else {
        _table_model->setTensorData(nullptr);
    }
}

void TensorDataView::removeCallbacks() {
    // TensorData view doesn't register callbacks currently
}

void TensorDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto tensor_data = dataManager()->getData<TensorData>(_active_key);
        _table_model->setTensorData(tensor_data.get());
    }
}

void TensorDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->setSortingEnabled(true);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view);
}

void TensorDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &TensorDataView::_handleTableViewDoubleClicked);
}

void TensorDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid()) {
        // Get the frame from the first column
        QVariant frame_data = _table_model->data(_table_model->index(index.row(), 0), Qt::DisplayRole);
        if (frame_data.isValid()) {
            emit frameSelected(frame_data.toInt());
        }
    }
}
