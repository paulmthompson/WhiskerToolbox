#ifndef ML_WIDGET_REGISTRATION_HPP
#define ML_WIDGET_REGISTRATION_HPP

/**
 * @file MLWidgetRegistration.hpp
 * @brief Registration function for ML_Widget editor type
 * 
 * This header provides a clean interface for registering the ML_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like MLWidgetState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "ML_Widget/MLWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     MLWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state and view (no separate properties)
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * 
 * ## Tool Widget Pattern
 * 
 * ML_Widget is a tool widget that:
 * - Goes to Zone::Right (properties zone) as a persistent tab
 * - Has no separate view/properties split (the widget is the tool)
 * - Is single-instance only
 * 
 * @see EditorRegistry for the type registration API
 * @see MLWidgetState for the state class
 * @see ML_Widget for the widget implementation
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace MLWidgetModule {

/**
 * @brief Register the ML_Widget editor type with the registry
 * 
 * This function registers the ML_Widget type, including:
 * - State factory: Creates MLWidgetState
 * - View factory: Creates ML_Widget (goes to Zone::Right)
 * - No properties factory (tool widget pattern)
 * 
 * ML_Widget is a single-instance tool widget placed in the right zone.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace MLWidgetModule

#endif  // ML_WIDGET_REGISTRATION_HPP
