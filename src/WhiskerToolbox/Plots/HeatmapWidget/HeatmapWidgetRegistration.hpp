#ifndef HEATMAP_WIDGET_REGISTRATION_HPP
#define HEATMAP_WIDGET_REGISTRATION_HPP

/**
 * @file HeatmapWidgetRegistration.hpp
 * @brief Registration function for Heatmap Widget editor types
 * 
 * This header provides a clean interface for registering the Heatmap Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like HeatmapState, HeatmapWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/HeatmapWidget/HeatmapWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     HeatmapWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see HeatmapState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace HeatmapWidgetModule {

/**
 * @brief Register all Heatmap Widget editor types with the registry
 * 
 * This function registers the HeatmapWidget type, including:
 * - State factory: Creates HeatmapState
 * - View factory: Creates HeatmapWidget (the main plot component)
 * - Properties factory: Creates HeatmapPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                  std::shared_ptr<DataManager> data_manager);

}  // namespace HeatmapWidgetModule

#endif  // HEATMAP_WIDGET_REGISTRATION_HPP
