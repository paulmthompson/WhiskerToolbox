#include "LineTableView.hpp"

#include "LineTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "Entity/EntityTypes.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QHeaderView>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

#include <set>
#include <unordered_set>

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

std::vector<EntityId> LineTableView::getSelectedEntityIds() const {
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
    _table_view->setContextMenuPolicy(Qt::CustomContextMenu);
    _table_view->horizontalHeader()->setStretchLastSection(true);

    _layout->addWidget(_table_view);
}

void LineTableView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &LineTableView::_handleTableViewDoubleClicked);
    connect(_table_view, &QTableView::customContextMenuRequested,
            this, &LineTableView::_showContextMenu);
}

void LineTableView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        auto tf = dataManager()->getData<LineData>(_active_key)->getTimeFrame();
        if (!tf) {
            std::cout << "LineTableView::_handleTableViewDoubleClicked: TimeFrame not found"
                      << _active_key << std::endl;
            return;
        }
        auto const row_data = _table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            emit frameSelected(TimePosition(row_data.frame, tf));
        }
    }
}

void LineTableView::_onDataChanged() {
    updateView();
}

void LineTableView::_showContextMenu(QPoint const & position) {
    QModelIndex const index = _table_view->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        emit moveLinesRequested(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        emit copyLinesRequested(target_key);
    };

    add_move_copy_submenus<LineData>(&context_menu, dataManager().get(), _active_key, move_callback, copy_callback);

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
                this, &LineTableView::removeLinesFromGroupRequested);
    }

    // Add separator and delete operation
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Line");
    connect(delete_action, &QAction::triggered,
            this, &LineTableView::deleteLinesRequested);

    context_menu.exec(_table_view->mapToGlobal(position));
}

void LineTableView::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
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
            emit moveLinesToGroupRequested(group_id);
        });
    }
}
