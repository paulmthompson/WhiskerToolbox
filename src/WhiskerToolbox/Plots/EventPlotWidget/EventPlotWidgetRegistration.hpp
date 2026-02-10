#ifndef EVENT_PLOT_WIDGET_REGISTRATION_HPP
#define EVENT_PLOT_WIDGET_REGISTRATION_HPP

/**
 * @file EventPlotWidgetRegistration.hpp
 * @brief Registration function for Event Plot Widget editor types
 * 
 * This header provides a clean interface for registering the Event Plot Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like EventPlotState, EventPlotWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/EventPlotWidget/EventPlotWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     EventPlotWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see EventPlotState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace EventPlotWidgetModule {

/**
 * @brief Register all Event Plot Widget editor types with the registry
 * 
 * This function registers the EventPlotWidget type, including:
 * - State factory: Creates EventPlotState
 * - View factory: Creates EventPlotWidget (the main plot component)
 * - Properties factory: Creates EventPlotPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace EventPlotWidgetModule

#endif  // EVENT_PLOT_WIDGET_REGISTRATION_HPP
