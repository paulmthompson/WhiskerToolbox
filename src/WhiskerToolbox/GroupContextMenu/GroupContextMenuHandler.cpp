#include "GroupContextMenuHandler.hpp"

#include "GroupManagementWidget/GroupManager.hpp"

#include <QAction>
#include <QMenu>

GroupContextMenuHandler::GroupContextMenuHandler(QObject * parent)
    : QObject(parent) {
}

GroupContextMenuHandler::~GroupContextMenuHandler() = default;

void GroupContextMenuHandler::setGroupManager(GroupManager * group_manager) {
    _group_manager = group_manager;
}

void GroupContextMenuHandler::setCallbacks(GroupContextMenuCallbacks const & callbacks) {
    _callbacks = callbacks;
}

void GroupContextMenuHandler::setupGroupMenuSection(QMenu * menu, bool add_trailing_separator) {
    if (!menu) {
        return;
    }
    
    // Create "Create New Group" action
    _action_create_new_group = new QAction("Create New Group", menu);
    connect(_action_create_new_group, &QAction::triggered, this, &GroupContextMenuHandler::onCreateNewGroup);
    menu->addAction(_action_create_new_group);
    
    // Add separator
    menu->addSeparator();
    
    // Create "Ungroup Selected" action
    _action_ungroup_selected = new QAction("Ungroup Selected", menu);
    connect(_action_ungroup_selected, &QAction::triggered, this, &GroupContextMenuHandler::onUngroupSelected);
    menu->addAction(_action_ungroup_selected);
    
    // Add trailing separator if requested
    if (add_trailing_separator) {
        menu->addSeparator();
    }
}

void GroupContextMenuHandler::updateMenuState(QMenu * menu) {
    if (!menu) {
        return;
    }
    
    bool has_selection = hasSelection();
    bool has_manager = hasGroupManager();
    
    // Update action states
    if (_action_create_new_group) {
        _action_create_new_group->setEnabled(has_selection && has_manager);
        _action_create_new_group->setVisible(has_manager);
    }
    
    if (_action_ungroup_selected) {
        _action_ungroup_selected->setEnabled(has_selection && has_manager);
        _action_ungroup_selected->setVisible(has_manager);
    }
    
    // Update dynamic group assignment submenu
    if (has_selection && has_manager) {
        updateDynamicGroupActions();
        
        // If we have dynamic actions, ensure they're in the menu
        if (!_dynamic_group_actions.empty() && !_assign_group_submenu) {
            // Create submenu if it doesn't exist
            _assign_group_submenu = menu->addMenu("Assign to Group");
            
            // Add dynamic actions to submenu
            for (QAction * action : _dynamic_group_actions) {
                _assign_group_submenu->addAction(action);
            }
        } else if (_assign_group_submenu) {
            // Update existing submenu
            _assign_group_submenu->clear();
            for (QAction * action : _dynamic_group_actions) {
                _assign_group_submenu->addAction(action);
            }
        }
    } else {
        // Hide submenu if no selection or no manager
        if (_assign_group_submenu) {
            _assign_group_submenu->menuAction()->setVisible(false);
        }
    }
    
    // Show submenu if we have it and it should be visible
    if (_assign_group_submenu && has_selection && has_manager && !_dynamic_group_actions.empty()) {
        _assign_group_submenu->menuAction()->setVisible(true);
    }
}

bool GroupContextMenuHandler::hasGroupManager() const {
    return _group_manager != nullptr;
}

bool GroupContextMenuHandler::hasSelection() const {
    if (_callbacks.hasSelection) {
        return _callbacks.hasSelection();
    }
    
    if (_callbacks.getSelectedEntities) {
        return !_callbacks.getSelectedEntities().empty();
    }
    
    return false;
}

void GroupContextMenuHandler::onCreateNewGroup() {
    if (!_group_manager || !_callbacks.getSelectedEntities) {
        return;
    }
    
    auto selected_entities = _callbacks.getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }
    
    int group_id = _group_manager->createGroupWithEntities(selected_entities);
    if (group_id != -1) {
        if (_callbacks.clearSelection) {
            _callbacks.clearSelection();
        }
        notifyGroupOperationCompleted();
    }
}

void GroupContextMenuHandler::onAssignToGroup(int group_id) {
    if (!_group_manager || !_callbacks.getSelectedEntities) {
        return;
    }
    
    auto selected_entities = _callbacks.getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }
    
    _group_manager->assignEntitiesToGroup(group_id, selected_entities);
    
    if (_callbacks.clearSelection) {
        _callbacks.clearSelection();
    }
    notifyGroupOperationCompleted();
}

void GroupContextMenuHandler::onUngroupSelected() {
    if (!_group_manager || !_callbacks.getSelectedEntities) {
        return;
    }
    
    auto selected_entities = _callbacks.getSelectedEntities();
    if (selected_entities.empty()) {
        return;
    }
    
    _group_manager->ungroupEntities(selected_entities);
    
    if (_callbacks.clearSelection) {
        _callbacks.clearSelection();
    }
    notifyGroupOperationCompleted();
}

void GroupContextMenuHandler::updateDynamicGroupActions() {
    // Clear existing dynamic actions
    for (QAction * action : _dynamic_group_actions) {
        action->deleteLater();
    }
    _dynamic_group_actions.clear();
    
    if (!_group_manager) {
        return;
    }
    
    // Get groups from the group manager
    auto groups = _group_manager->getGroupsForContextMenu();
    
    // Create actions for each group
    for (auto const & [group_id, group_name] : groups) {
        QAction * assign_action = new QAction(QString("Assign to %1").arg(group_name), this);
        
        connect(assign_action, &QAction::triggered, this, [this, group_id]() {
            onAssignToGroup(group_id);
        });
        
        _dynamic_group_actions.push_back(assign_action);
    }
}

void GroupContextMenuHandler::notifyGroupOperationCompleted() {
    if (_callbacks.onGroupOperationCompleted) {
        _callbacks.onGroupOperationCompleted();
    }
}
