#ifndef LINE_PLOT_WIDGET_REGISTRATION_HPP
#define LINE_PLOT_WIDGET_REGISTRATION_HPP

/**
 * @file LinePlotWidgetRegistration.hpp
 * @brief Registration function for Line Plot Widget editor types
 * 
 * This header provides a clean interface for registering the Line Plot Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like LinePlotState, LinePlotWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/LinePlotWidget/LinePlotWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     LinePlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see LinePlotState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace LinePlotWidgetModule {

/**
 * @brief Register all Line Plot Widget editor types with the registry
 * 
 * This function registers the LinePlotWidget type, including:
 * - State factory: Creates LinePlotState
 * - View factory: Creates LinePlotWidget (the main plot component)
 * - Properties factory: Creates LinePlotPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace LinePlotWidgetModule

#endif  // LINE_PLOT_WIDGET_REGISTRATION_HPP
