#ifndef ACF_WIDGET_REGISTRATION_HPP
#define ACF_WIDGET_REGISTRATION_HPP

/**
 * @file ACFWidgetRegistration.hpp
 * @brief Registration function for ACF Widget editor types
 * 
 * This header provides a clean interface for registering the ACF Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like ACFState, ACFWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/ACFWidget/ACFWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     ACFWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state, view, and properties
 * - Type metadata (display name, menu path, default zone)
 * - Complex widget creation logic
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * 
 * @see EditorRegistry for the type registration API
 * @see ACFState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace ACFWidgetModule {

/**
 * @brief Register all ACF Widget editor types with the registry
 * 
 * This function registers the ACFWidget type, including:
 * - State factory: Creates ACFState
 * - View factory: Creates ACFWidget (the main plot component)
 * - Properties factory: Creates ACFPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace ACFWidgetModule

#endif  // ACF_WIDGET_REGISTRATION_HPP
