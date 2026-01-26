#ifndef TABLE_DESIGNER_WIDGET_REGISTRATION_HPP
#define TABLE_DESIGNER_WIDGET_REGISTRATION_HPP

/**
 * @file TableDesignerWidgetRegistration.hpp
 * @brief Registration function for TableDesignerWidget editor type
 * 
 * This header provides a clean interface for registering the TableDesignerWidget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TableDesignerState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "TableDesignerWidget/TableDesignerWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TableDesignerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design
 * 
 * TableDesignerWidget is a pure properties widget for creating and configuring
 * table views. It provides tools for:
 * - Creating new tables with row selectors (intervals, events)
 * - Adding columns with various computers (Mean, Max, etc.)
 * - Managing computer states (enabled/disabled, custom names)
 * - Exporting and transforming table data
 * 
 * Key characteristics:
 * - Single instance only (allow_multiple = false)
 * - Placed in Zone::Right (properties zone)
 * - No separate view - this IS the view
 * - No properties panel (create_properties = nullptr)
 * 
 * @see EditorRegistry for the type registration API
 * @see TableDesignerState for the shared state class
 * @see TableDesignerWidget for the widget implementation
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace TableDesignerWidgetModule {

/**
 * @brief Register the TableDesignerWidget editor type with the registry
 * 
 * This function registers the TableDesignerWidget type, including:
 * - State factory: Creates TableDesignerState
 * - View factory: Creates TableDesignerWidget (goes to Zone::Right)
 * - No properties factory (self-contained properties widget)
 * 
 * TableDesignerWidget is a single-instance utility widget for designing
 * table views.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager instance (required for table data)
 */
void registerTypes(EditorRegistry * registry, std::shared_ptr<DataManager> data_manager);

}  // namespace TableDesignerWidgetModule

#endif  // TABLE_DESIGNER_WIDGET_REGISTRATION_HPP
