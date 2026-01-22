#ifndef GROUP_MANAGEMENT_WIDGET_REGISTRATION_HPP
#define GROUP_MANAGEMENT_WIDGET_REGISTRATION_HPP

/**
 * @file GroupManagementWidgetRegistration.hpp
 * @brief Registration function for GroupManagementWidget editor type
 * 
 * This header provides a clean interface for registering the GroupManagementWidget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like GroupManagementWidgetState, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "GroupManagementWidget/GroupManagementWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     GroupManagementWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager.get());
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state and view (no separate properties widget)
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow.
 * 
 * ## Zone Placement
 * 
 * GroupManagementWidget is registered with:
 * - preferred_zone = Zone::Left (navigation/group management panel)
 * - properties_zone = Zone::Left (no separate properties)
 * - allow_multiple = false (single instance, central group management)
 * 
 * The widget provides group overview and management interface in the left zone,
 * positioned above the DataManager_Widget.
 * 
 * @see EditorRegistry for the type registration API
 * @see GroupManagementWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;
class GroupManager;

namespace GroupManagementWidgetModule {

/**
 * @brief Register the GroupManagementWidget editor type with the registry
 * 
 * This function registers the GroupManagementWidget type, including:
 * - State factory: Creates GroupManagementWidgetState
 * - View factory: Creates the GroupManagementWidget (no separate properties)
 * 
 * The widget requires:
 * - GroupManager: For entity group management operations
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction (used by GroupManager)
 * @param group_manager GroupManager for entity groups (required)
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager);

}  // namespace GroupManagementWidgetModule

#endif // GROUP_MANAGEMENT_WIDGET_REGISTRATION_HPP
