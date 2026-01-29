#include "PointTableView.hpp"

#include "PointTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Points/Point_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

PointTableView::PointTableView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new PointTableModel(this)) {
    _setupUi();
    _connectSignals();
}

PointTableView::~PointTableView() {
    removeCallbacks();
}

void PointTableView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<PointData>(key)) {
        return;  // Already set to this key
    }

    removeCallbacks();
    _active_key = key;

    auto point_data = dataManager()->getData<PointData>(_active_key);
    if (point_data) {
        _table_model->setPoints(point_data.get());
        _callback_id = point_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setPoints(nullptr);
    }
}

void PointTableView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void PointTableView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto point_data = dataManager()->getData<PointData>(_active_key);
        _table_model->setPoints(point_data.get());
    }
}

void PointTableView::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    if (_table_model) {
        _table_model->setGroupManager(group_manager);
    }
}

void PointTableView::setGroupFilter(int group_id) {
    if (_table_model) {
        _table_model->setGroupFilter(group_id);
    }
}

void PointTableView::clearGroupFilter() {
    if (_table_model) {
        _table_model->clearGroupFilter();
    }
}

std::vector<int64_t> PointTableView::getSelectedFrames() const {
    std::vector<int64_t> frames;
    
    if (!_table_view || !_table_model) {
        return frames;
    }

    auto const selection = _table_view->selectionModel()->selectedRows();
    frames.reserve(static_cast<size_t>(selection.size()));
    
    for (auto const & index : selection) {
        auto const row_data = _table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            frames.push_back(row_data.frame);
        }
    }

    return frames;
}

void PointTableView::_setupUi() {
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

void PointTableView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &PointTableView::_handleTableViewDoubleClicked);
}

void PointTableView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        auto tf = dataManager()->getTime(TimeKey(_active_key));
        auto const row_data = _table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            emit frameSelected(TimePosition(row_data.frame, tf));
        }
    }
}

void PointTableView::_onDataChanged() {
    updateView();
}
