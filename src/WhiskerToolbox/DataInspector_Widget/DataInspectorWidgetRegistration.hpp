#ifndef DATA_INSPECTOR_WIDGET_REGISTRATION_HPP
#define DATA_INSPECTOR_WIDGET_REGISTRATION_HPP

/**
 * @file DataInspectorWidgetRegistration.hpp
 * @brief Registration function for Data Inspector editor types
 * 
 * This header provides a clean interface for registering the DataInspector
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "DataInspector_Widget/DataInspectorWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     DataInspectorModule::registerTypes(_editor_registry.get(), _data_manager, _group_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state, view, and properties
 * - Type metadata (display name, menu path, default zones)
 * - Complex widget creation logic (view/properties coupling via shared state)
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * 
 * @see EditorRegistry for the type registration API
 * @see DataInspectorState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;
class GroupManager;

namespace DataInspectorModule {

/**
 * @brief Register all Data Inspector editor types with the registry
 * 
 * This function registers the DataInspector type, including:
 * - State factory: Creates DataInspectorState
 * - View factory: Creates DataInspectorViewWidget (Center zone)
 * - Properties factory: Creates DataInspectorPropertiesWidget (Right zone)
 * 
 * The Data Inspector supports multiple instances (allow_multiple = true).
 * Each instance can be pinned to inspect a specific data key regardless
 * of SelectionContext.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for data access
 * @param group_manager Optional GroupManager for group-aware features (can be nullptr)
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager = nullptr);

}  // namespace DataInspectorModule

#endif  // DATA_INSPECTOR_WIDGET_REGISTRATION_HPP
