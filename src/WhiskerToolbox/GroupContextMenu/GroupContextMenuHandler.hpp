#ifndef GROUPCONTEXTMENUHANDLER_HPP
#define GROUPCONTEXTMENUHANDLER_HPP

#include "Entity/EntityTypes.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

class QMenu;
class QAction;
class GroupManager;

/**
 * @brief Callback structure for group-related actions
 * 
 * Widgets using this handler should provide implementations for these callbacks
 * to perform the actual operations on their selected entities.
 */
struct GroupContextMenuCallbacks {
    /**
     * @brief Get the currently selected entity IDs
     * @return Set of selected EntityIds
     */
    std::function<std::unordered_set<EntityId>()> getSelectedEntities;
    
    /**
     * @brief Clear the current selection in the widget
     */
    std::function<void()> clearSelection;
    
    /**
     * @brief Optional: Check if the widget has any selected entities
     * If not provided, will use !getSelectedEntities().empty()
     */
    std::function<bool()> hasSelection;
    
    /**
     * @brief Optional: Called after a group operation is completed
     * Can be used to trigger a redraw or update
     */
    std::function<void()> onGroupOperationCompleted;
};

/**
 * @brief Manages group-related context menu actions
 * 
 * This class provides a reusable way to add group management functionality
 * to context menus in different widgets. It handles:
 * - Creating "Create New Group" actions
 * - Dynamically populating "Assign to Group" submenus
 * - Providing "Ungroup Selected" functionality
 * - Managing action states based on selection and group manager availability
 * 
 * Usage:
 * 1. Create a GroupContextMenuHandler instance
 * 2. Set the GroupManager via setGroupManager()
 * 3. Set callbacks via setCallbacks()
 * 4. Call setupGroupMenuSection() to add group actions to your menu
 * 5. Call updateMenuState() before showing the context menu
 */
class GroupContextMenuHandler : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a new GroupContextMenuHandler
     * @param parent Optional parent QObject
     */
    explicit GroupContextMenuHandler(QObject * parent = nullptr);
    
    ~GroupContextMenuHandler() override;
    
    /**
     * @brief Set the GroupManager to use for group operations
     * @param group_manager Pointer to the GroupManager (not owned)
     */
    void setGroupManager(GroupManager * group_manager);
    
    /**
     * @brief Set the callbacks for group operations
     * @param callbacks Structure containing callback functions
     */
    void setCallbacks(GroupContextMenuCallbacks const & callbacks);
    
    /**
     * @brief Add group-related actions to a menu
     * 
     * This adds:
     * - "Create New Group" action
     * - Separator
     * - "Ungroup Selected" action
     * - Separator (if add_trailing_separator is true)
     * - Dynamic "Assign to Group" submenu (populated via updateMenuState)
     * 
     * @param menu The menu to add actions to
     * @param add_trailing_separator If true, adds a separator after the group section
     */
    void setupGroupMenuSection(QMenu * menu, bool add_trailing_separator = false);
    
    /**
     * @brief Update the menu state based on current selection and available groups
     * 
     * This should be called before showing the context menu. It:
     * - Enables/disables actions based on whether there is a selection
     * - Shows/hides actions based on whether there is a group manager
     * - Populates the dynamic group assignment submenu with current groups
     * 
     * @param menu The menu to update (should be the same menu passed to setupGroupMenuSection)
     */
    void updateMenuState(QMenu * menu);
    
    /**
     * @brief Check if the handler has a valid group manager
     * @return True if a group manager is set
     */
    [[nodiscard]] bool hasGroupManager() const;
    
    /**
     * @brief Check if there is a current selection (uses callbacks)
     * @return True if there are selected entities
     */
    [[nodiscard]] bool hasSelection() const;

private slots:
    /**
     * @brief Handle "Create New Group" action
     */
    void onCreateNewGroup();
    
    /**
     * @brief Handle "Assign to Group" action
     * @param group_id The ID of the group to assign entities to
     */
    void onAssignToGroup(int group_id);
    
    /**
     * @brief Handle "Ungroup Selected" action
     */
    void onUngroupSelected();

private:
    GroupManager * _group_manager = nullptr;
    GroupContextMenuCallbacks _callbacks;
    
    // Actions (owned by the menu)
    QAction * _action_create_new_group = nullptr;
    QAction * _action_ungroup_selected = nullptr;
    QMenu * _assign_group_submenu = nullptr;
    
    // Track dynamic actions for cleanup
    std::vector<QAction *> _dynamic_group_actions;
    
    /**
     * @brief Clear and repopulate the dynamic group assignment actions
     */
    void updateDynamicGroupActions();
    
    /**
     * @brief Invoke the onGroupOperationCompleted callback if set
     */
    void notifyGroupOperationCompleted();
};

#endif // GROUPCONTEXTMENUHANDLER_HPP
