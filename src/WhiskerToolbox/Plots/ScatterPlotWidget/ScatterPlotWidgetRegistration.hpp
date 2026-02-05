#ifndef SCATTER_PLOT_WIDGET_REGISTRATION_HPP
#define SCATTER_PLOT_WIDGET_REGISTRATION_HPP

/**
 * @file ScatterPlotWidgetRegistration.hpp
 * @brief Registration function for Scatter Plot Widget editor types
 * 
 * This header provides a clean interface for registering the Scatter Plot Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like ScatterPlotState, ScatterPlotWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/ScatterPlotWidget/ScatterPlotWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     ScatterPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see ScatterPlotState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace ScatterPlotWidgetModule {

/**
 * @brief Register all Scatter Plot Widget editor types with the registry
 * 
 * This function registers the ScatterPlotWidget type, including:
 * - State factory: Creates ScatterPlotState
 * - View factory: Creates ScatterPlotWidget (the main plot component)
 * - Properties factory: Creates ScatterPlotPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace ScatterPlotWidgetModule

#endif  // SCATTER_PLOT_WIDGET_REGISTRATION_HPP
