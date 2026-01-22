#ifndef DATA_TRANSFORM_WIDGET_REGISTRATION_HPP
#define DATA_TRANSFORM_WIDGET_REGISTRATION_HPP

/**
 * @file DataTransformWidgetRegistration.hpp
 * @brief Registration function for DataTransformWidget editor type
 * 
 * This header provides a clean interface for registering the DataTransformWidget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like DataTransformWidgetState, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "DataTransform_Widget/DataTransformWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     DataTransformWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * ## Phase 4 Refactoring Notes
 * 
 * DataTransformWidget is registered with:
 * - preferred_zone = Zone::Right (it IS the properties widget)
 * - properties_zone = Zone::Right (no separate properties)
 * - allow_multiple = false (single instance)
 * 
 * The widget implements DataFocusAware to respond to selection changes
 * from other widgets via SelectionContext::dataFocusChanged.
 * 
 * @see EditorRegistry for the type registration API
 * @see DataTransformWidgetState for the shared state class
 * @see DataFocusAware for passive awareness pattern
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace DataTransformWidgetModule {

/**
 * @brief Register the DataTransformWidget editor type with the registry
 * 
 * This function registers the DataTransformWidget type, including:
 * - State factory: Creates DataTransformWidgetState
 * - View factory: Creates the DataTransform_Widget (no separate properties)
 * 
 * DataTransformWidget is a single-widget editor (no view/properties split)
 * that lives in Zone::Right as a persistent tool tab.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace DataTransformWidgetModule

#endif  // DATA_TRANSFORM_WIDGET_REGISTRATION_HPP
