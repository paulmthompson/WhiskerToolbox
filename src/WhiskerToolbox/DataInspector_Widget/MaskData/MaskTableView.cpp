#include "MaskTableView.hpp"

#include "MaskTableModel.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"
#include "Entity/EntityTypes.hpp"
#include "WhiskerToolbox/GroupManagementWidget/GroupManager.hpp"

#include <QAction>
#include <QHeaderView>
#include <QMenu>
#include <QTableView>
#include <QVBoxLayout>

#include <set>
#include <string>

MaskTableView::MaskTableView(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : BaseDataView(std::move(data_manager), parent)
    , _table_model(new MaskTableModel(this)) {
    _setupUi();
    _connectSignals();
}

MaskTableView::~MaskTableView() {
    removeCallbacks();
}

void MaskTableView::setActiveKey(std::string const & key) {
    if (_active_key == key && dataManager()->getData<MaskData>(key)) {
        return;
    }

    removeCallbacks();
    _active_key = key;

    auto mask_data = dataManager()->getData<MaskData>(_active_key);
    if (mask_data) {
        _table_model->setMasks(mask_data.get());
        _callback_id = mask_data->addObserver([this]() { _onDataChanged(); });
    } else {
        _table_model->setMasks(nullptr);
    }
}

void MaskTableView::removeCallbacks() {
    remove_callback(dataManager().get(), _active_key, _callback_id);
}

void MaskTableView::updateView() {
    if (!_active_key.empty() && _table_model) {
        auto mask_data = dataManager()->getData<MaskData>(_active_key);
        _table_model->setMasks(mask_data.get());
    }
}

void MaskTableView::setGroupManager(GroupManager * group_manager) {
    // Disconnect from old group manager
    if (_group_manager) {
        disconnect(_group_manager, nullptr, this, nullptr);
    }

    _group_manager = group_manager;
    if (_table_model) {
        _table_model->setGroupManager(group_manager);
    }

    // Connect to new group manager signals
    if (_group_manager) {
        connect(_group_manager, &GroupManager::groupCreated,
                this, &MaskTableView::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupRemoved,
                this, &MaskTableView::_onGroupChanged);
        connect(_group_manager, &GroupManager::groupModified,
                this, &MaskTableView::_onGroupChanged);
    }
}

void MaskTableView::setGroupFilter(int group_id) {
    if (_table_model) {
        _table_model->setGroupFilter(group_id);
    }
}

void MaskTableView::clearGroupFilter() {
    if (_table_model) {
        _table_model->clearGroupFilter();
    }
}

std::vector<int64_t> MaskTableView::getSelectedFrames() const {
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

std::vector<EntityId> MaskTableView::getSelectedEntityIds() const {
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

void MaskTableView::_setupUi() {
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

void MaskTableView::_connectSignals() {
    connect(_table_view, &QTableView::doubleClicked,
            this, &MaskTableView::_handleTableViewDoubleClicked);
    connect(_table_view, &QTableView::customContextMenuRequested,
            this, &MaskTableView::_showContextMenu);
}

void MaskTableView::_handleTableViewDoubleClicked(QModelIndex const & index) {
    if (index.isValid() && _table_model) {

        auto tf = dataManager()->getData<MaskData>(_active_key)->getTimeFrame();
        if (!tf) {
            std::cout << "MaskTableView::_handleTableViewDoubleClicked: TimeFrame not found"
                      << _active_key << std::endl;
            return;
        }
        auto const row_data = _table_model->getRowData(index.row());
        if (row_data.frame != -1) {
            emit frameSelected(TimePosition(row_data.frame, tf));
        }
    }
}

void MaskTableView::_onDataChanged() {
    updateView();
}

void MaskTableView::_onGroupChanged() {
    // Refresh the view to update group information and reapply filters
    updateView();
}

void MaskTableView::_showContextMenu(QPoint const & position) {
    QModelIndex const index = _table_view->indexAt(position);
    if (!index.isValid()) {
        return;
    }

    QMenu context_menu(this);

    // Add move and copy submenus using the utility function
    auto move_callback = [this](std::string const & target_key) {
        emit moveMasksRequested(target_key);
    };

    auto copy_callback = [this](std::string const & target_key) {
        emit copyMasksRequested(target_key);
    };

    add_move_copy_submenus<MaskData>(&context_menu, dataManager().get(), _active_key, move_callback, copy_callback);

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
                this, &MaskTableView::removeMasksFromGroupRequested);
    }

    // Add separator and delete operation
    context_menu.addSeparator();
    QAction * delete_action = context_menu.addAction("Delete Selected Mask");
    connect(delete_action, &QAction::triggered,
            this, &MaskTableView::deleteMasksRequested);

    context_menu.exec(_table_view->mapToGlobal(position));
}

void MaskTableView::_populateGroupSubmenu(QMenu * menu, bool for_moving) {
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
            emit moveMasksToGroupRequested(group_id);
        });
    }
}
