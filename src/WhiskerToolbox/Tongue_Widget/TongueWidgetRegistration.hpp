#ifndef TONGUE_WIDGET_REGISTRATION_HPP
#define TONGUE_WIDGET_REGISTRATION_HPP

/**
 * @file TongueWidgetRegistration.hpp
 * @brief Registration function for Tongue_Widget editor type
 * 
 * This header provides a clean interface for registering the Tongue_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TongueWidgetState, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Tongue_Widget/TongueWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TongueWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * ## Zone Configuration
 * 
 * Tongue_Widget is registered with:
 * - preferred_zone = Zone::Right (tool widget)
 * - properties_zone = Zone::Right (no separate properties)
 * - allow_multiple = false (single instance)
 * - auto_raise_properties = true (raised when opened)
 * 
 * @see EditorRegistry for the type registration API
 * @see TongueWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace TongueWidgetModule {

/**
 * @brief Register the Tongue_Widget editor type with the registry
 * 
 * This function registers the TongueWidget type, including:
 * - State factory: Creates TongueWidgetState
 * - View factory: Creates the Tongue_Widget (no separate properties)
 * 
 * Tongue_Widget is a single-widget editor (no view/properties split)
 * that lives in Zone::Right as a tool widget.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace TongueWidgetModule

#endif  // TONGUE_WIDGET_REGISTRATION_HPP
