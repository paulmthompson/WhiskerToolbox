#ifndef DATA_IMPORT_WIDGET_REGISTRATION_HPP
#define DATA_IMPORT_WIDGET_REGISTRATION_HPP

/**
 * @file DataImportWidgetRegistration.hpp
 * @brief Registration function for DataImport_Widget editor type
 * 
 * This header provides a clean interface for registering the DataImport_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like DataImportWidgetState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "DataImport_Widget/DataImportWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     DataImportWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state and view (no separate properties widget)
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic including SelectionContext connection
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow's includes.
 * 
 * ## Zone Placement
 * 
 * DataImport_Widget is configured for:
 * - preferred_zone = Zone::Right (properties panel location)
 * - allow_multiple = false (single instance only)
 * - properties_as_tab = true (add as tab in the zone)
 * - auto_raise_properties = false (don't steal focus)
 * 
 * ## Passive Awareness
 * 
 * The widget implements DataFocusAware and automatically responds to
 * SelectionContext::dataFocusChanged by switching to the appropriate
 * loader widget for the focused data type.
 * 
 * @see DataImport_Widget for the main widget implementation
 * @see DataImportWidgetState for state management
 * @see DataFocusAware for passive awareness pattern
 */

#include <memory>

// Forward declarations
class DataManager;
class EditorRegistry;

namespace DataImportWidgetModule {

/**
 * @brief Register the DataImport_Widget editor type with the registry
 * 
 * This function registers the DataImport_Widget type, including:
 * - State factory: Creates DataImportWidgetState
 * - View factory: Creates DataImport_Widget (goes to Zone::Right)
 * - No properties factory (self-contained tool widget pattern)
 * 
 * DataImport_Widget is a single-instance tool widget that responds to
 * data focus changes by showing the appropriate import interface.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace DataImportWidgetModule

#endif  // DATA_IMPORT_WIDGET_REGISTRATION_HPP
