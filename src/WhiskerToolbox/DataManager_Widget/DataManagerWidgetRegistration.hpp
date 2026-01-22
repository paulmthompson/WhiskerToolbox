#ifndef DATA_MANAGER_WIDGET_REGISTRATION_HPP
#define DATA_MANAGER_WIDGET_REGISTRATION_HPP

/**
 * @file DataManagerWidgetRegistration.hpp
 * @brief Registration function for DataManager_Widget editor type
 * 
 * This header provides a clean interface for registering the DataManager_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like DataManagerWidgetState, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "DataManager_Widget/DataManagerWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     DataManagerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar, _group_manager.get());
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
 * DataManager_Widget is registered with:
 * - preferred_zone = Zone::Left (navigation/data selection panel)
 * - properties_zone = Zone::Left (no separate properties)
 * - allow_multiple = false (single instance, central data view)
 * 
 * The widget provides a data overview and selection interface that
 * broadcasts selection changes to other widgets via SelectionContext.
 * 
 * @see EditorRegistry for the type registration API
 * @see DataManagerWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;
class TimeScrollBar;
class GroupManager;

namespace DataManagerWidgetModule {

/**
 * @brief Register the DataManager_Widget editor type with the registry
 * 
 * This function registers the DataManager_Widget type, including:
 * - State factory: Creates DataManagerWidgetState
 * - View factory: Creates the DataManager_Widget (no separate properties)
 * 
 * The widget requires additional dependencies beyond just DataManager:
 * - TimeScrollBar: For time navigation
 * - GroupManager: For entity group management
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 * @param time_scrollbar TimeScrollBar for time navigation (can be nullptr)
 * @param group_manager GroupManager for entity groups (can be nullptr)
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   TimeScrollBar * time_scrollbar,
                   GroupManager * group_manager);

}  // namespace DataManagerWidgetModule

#endif // DATA_MANAGER_WIDGET_REGISTRATION_HPP
