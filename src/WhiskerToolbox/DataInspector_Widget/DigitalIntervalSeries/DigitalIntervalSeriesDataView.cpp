#include "DigitalIntervalSeriesDataView.hpp"

#include "IntervalTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "Entity/EntityTypes.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QHeaderView>
#include <QItemSelectionModel>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

#include <iostream>
#include <set>
#include <unordered_set>

DigitalIntervalSeriesDataView::DigitalIntervalSeriesDataView(
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : BaseDataView(std::move(data_manager), parent),
      _table_model(new IntervalTableModel(this)) {
    _setupUi();
    _connectSignals();
}

DigitalIntervalSeriesDataView::~DigitalIntervalSeriesDataView() {
    removeCallbacks();
    if (_group_manager) {
        disconnect(_group_manager, nullptr, this, nullptr);
    }
}

void DigitalIntervalSeriesDataView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<DigitalIntervalSeries>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
    if (interval_data) {
        _table_model->setIntervals(interval_data.get());
        _callback_id = interval_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setIntervals(nullptr);
    }
}

void DigitalIntervalSeriesDataView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void DigitalIntervalSeriesDataView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto interval_data = dataManager()->getData<DigitalIntervalSeries>(_active_key);
        _table_model->setIntervals(interval_data.get());
    }
}

void DigitalIntervalSeriesDataView::setGroupManager(GroupManager * group_manager) {
    if (_group_manager) {
        disconnect(_group_manager, nullptr, this, nullptr);
    }

    _group_manager = group_manager;
    if (_table_model) {
        _table_model->setGroupManager(group_manager);
    }

    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated,
                this, &DigitalIntervalSeriesDataView::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved,
                this, &DigitalIntervalSeriesDataView::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified,
                this, &DigitalIntervalSeriesDataView::_onGroupChanged);
    }
}

void DigitalIntervalSeriesDataView::setGroupFilter(int group_id) {
    if (_table_model) {
        _table_model->setGroupFilter(group_id);
    }
}

void DigitalIntervalSeriesDataView::clearGroupFilter() {
    if (_table_model) {
        _table_model->clearGroupFilter();
    }
}

void DigitalIntervalSeriesDataView::_setupUi() {
    _layout = new QVBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _table_view = new QTableView(this);
    _table_view->setModel(_table_model);
    _table_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _table_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table_view->setAlternatingRowColors(true);
    _table_view->setSortingEnabled(true);
    _table_view->setContextMenuPolicy(Qt::CustomContextMenu);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view);
}

void DigitalIntervalSeriesDataView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &DigitalIntervalSeriesDataView::_handleTableViewDoubleClicked);
    connect(_table_view, &QTableView::customContextMenuRequested,
            this, &DigitalIntervalSeriesDataView::_showContextMenu);
}

void DigitalIntervalSeriesDataView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        auto tf = dataManager()->getData<DigitalIntervalSeries>(_active_key)->getTimeFrame();
        if (!tf) {
            std::cout << "DigitalIntervalSeriesDataView::_handleTableViewDoubleClicked: TimeFrame not found"
                      << _active_key << std::endl;
            return;
        }
        auto interval = _table_model->getInterval(index.row());
        // Navigate to start (column 0) or end (column 1) based on which cell was clicked
        int64_t target_frame = (index.column() == 0) ? interval.start : interval.end;
        emit frameSelected(TimePosition(target_frame, tf));
    }
}

void DigitalIntervalSeriesDataView::_onDataChanged() {
    updateView();
}

void DigitalIntervalSeriesDataView::_onGroupChanged() {
    updateView();
}

std::vector<Interval> DigitalIntervalSeriesDataView::getSelectedIntervals() const {
    std::vector<Interval> selected_intervals;
    if (!_table_view || !_table_model) {
        return selected_intervals;
    }

    QModelIndexList selected_indexes = _table_view->selectionModel()->selectedRows();
    for (QModelIndex const & index: selected_indexes) {
        if (index.isValid()) {
            Interval interval = _table_model->getInterval(index.row());
            selected_intervals.push_back(interval);
        }
    }

    return selected_intervals;
}

std::vector<EntityId> DigitalIntervalSeriesDataView::getSelectedEntityIds() const {
    std::vector<EntityId> entity_ids;

    if (!_table_view || !_table_model) {
        return entity_ids;
    }

    auto const selection = _table_view->selectionModel()->selectedRows();
    entity_ids.reserve(static_cast<size_t>(selection.size()));

    for (auto const & index : selection) {
        if (index.isValid()) {
            auto const row_data = _table_model->getRowData(index.row());
            if (row_data.entity_id != EntityId(0)) {
                entity_ids.push_back(row_data.entity_id);
            }
        }
    }

    return entity_ids;
}

void DigitalIntervalSeriesDataView::_showContextMenu(QPoint const & position) {
    QModelIndex const index = _table_view->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        emit moveIntervalsRequested(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        emit copyIntervalsRequested(target_key);
    };

    add_move_copy_submenus<DigitalIntervalSeries>(&context_menu, dataManager().get(), _active_key, move_callback, copy_callback);

    // Add group management options
    if (_group_manager) {
        context_menu.addSeparator();
        QMenu * group_menu = context_menu.addMenu("Group Management");

        // Add "Move to Group" submenu
        QMenu * move_to_group_menu = group_menu->addMenu("Move to Group");
        _populateGroupSubmenu(move_to_group_menu, true);

        // Add "Remove from Group" action
        QAction * remove_from_group_action = group_menu->addAction("Remove from Group");
        connect(remove_from_group_action, &QAction::triggered,
                this, &DigitalIntervalSeriesDataView::removeIntervalsFromGroupRequested);
    }

    // Add separator and delete operation
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Interval");
    connect(delete_action, &QAction::triggered,
            this, &DigitalIntervalSeriesDataView::deleteIntervalsRequested);

    context_menu.exec(_table_view->mapToGlobal(position));
}

void DigitalIntervalSeriesDataView::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
    if (!_group_manager) {
        return;
    }

    // Get current groups of selected entities to exclude them from the move list
    std::set<int> current_groups;
    if (for_moving) {
        auto const selection = _table_view->selectionModel()->selectedRows();
        for (auto const & index : selection) {
            if (index.isValid()) {
                auto const row_data = _table_model->getRowData(index.row());
                if (row_data.entity_id != EntityId(0)) {
                    int current_group = _group_manager->getEntityGroup(row_data.entity_id);
                    if (current_group != -1) {
                        current_groups.insert(current_group);
                    }
                }
            }
        }
    }

    auto groups = _group_manager->getGroups();
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        int group_id = it.key();
        QString group_name = it.value().name;

        // Skip current groups when moving
        if (for_moving && current_groups.find(group_id) != current_groups.end()) {
            continue;
        }

        QAction * action = menu->addAction(group_name);
        connect(action, &QAction::triggered, this, [this, group_id]() {
            emit moveIntervalsToGroupRequested(group_id);
        });
    }
}
