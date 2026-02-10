#ifndef THREE_D_PLOT_WIDGET_REGISTRATION_HPP
#define THREE_D_PLOT_WIDGET_REGISTRATION_HPP

/**
 * @file 3DPlotWidgetRegistration.hpp
 * @brief Registration function for 3D Plot Widget editor types
 * 
 * This header provides a clean interface for registering the 3D Plot Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like ThreeDPlotState, ThreeDPlotWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/3DPlot/3DPlotWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     ThreeDPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see ThreeDPlotState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace ThreeDPlotWidgetModule {

/**
 * @brief Register all 3D Plot Widget editor types with the registry
 * 
 * This function registers the ThreeDPlotWidget type, including:
 * - State factory: Creates ThreeDPlotState
 * - View factory: Creates ThreeDPlotWidget (the main plot component)
 * - Properties factory: Creates ThreeDPlotPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace ThreeDPlotWidgetModule

#endif  // THREE_D_PLOT_WIDGET_REGISTRATION_HPP
