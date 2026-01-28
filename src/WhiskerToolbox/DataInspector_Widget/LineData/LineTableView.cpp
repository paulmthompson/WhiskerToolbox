#include "LineTableView.hpp"

#include "LineTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include <QHeaderView>
#include <QTableView>
#include <QVBoxLayout>

LineTableView::LineTableView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new LineTableModel(this)) {
    _setupUi();
    _connectSignals();
}

LineTableView::~LineTableView() {
    removeCallbacks();
}

void LineTableView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<LineData>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto line_data = dataManager()->getData<LineData>(_active_key);
    if (line_data) {
        _table_model->setLines(line_data.get());
        _callback_id = line_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setLines(nullptr);
    }
}

void LineTableView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void LineTableView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto line_data = dataManager()->getData<LineData>(_active_key);
        _table_model->setLines(line_data.get());
    }
}

void LineTableView::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
    if (_table_model) {
        _table_model->setGroupManager(group_manager);
    }
}

void LineTableView::setGroupFilter(int group_id) {
    if (_table_model) {
        _table_model->setGroupFilter(group_id);
    }
}

void LineTableView::clearGroupFilter() {
    if (_table_model) {
        _table_model->clearGroupFilter();
    }
}

std::vector<int64_t> LineTableView::getSelectedFrames() const {
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

void LineTableView::scrollToFrame(int64_t frame) {
    if (!_table_view || !_table_model) {
        return;
    }

    int row = _table_model->findRowForFrame(frame);
    if (row >= 0) {
        _table_view->scrollTo(_table_model->index(row, 0));
        _table_view->selectRow(row);
    }
}

void LineTableView::_setupUi() {
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

void LineTableView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &LineTableView::_handleTableViewDoubleClicked);
}

void LineTableView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {
        auto const row_data = _table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            emit frameSelected(static_cast<int>(row_data.frame));
        }
    }
}

void LineTableView::_onDataChanged() {
    updateView();
}
