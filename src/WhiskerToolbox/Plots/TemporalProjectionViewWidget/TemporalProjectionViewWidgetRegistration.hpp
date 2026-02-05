#ifndef TEMPORAL_PROJECTION_VIEW_WIDGET_REGISTRATION_HPP
#define TEMPORAL_PROJECTION_VIEW_WIDGET_REGISTRATION_HPP

/**
 * @file TemporalProjectionViewWidgetRegistration.hpp
 * @brief Registration function for Temporal Projection View Widget editor types
 * 
 * This header provides a clean interface for registering the Temporal Projection View Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TemporalProjectionViewState, TemporalProjectionViewWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/TemporalProjectionViewWidget/TemporalProjectionViewWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TemporalProjectionViewWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see TemporalProjectionViewState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace TemporalProjectionViewWidgetModule {

/**
 * @brief Register all Temporal Projection View Widget editor types with the registry
 * 
 * This function registers the TemporalProjectionViewWidget type, including:
 * - State factory: Creates TemporalProjectionViewState
 * - View factory: Creates TemporalProjectionViewWidget (the main plot component)
 * - Properties factory: Creates TemporalProjectionViewPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace TemporalProjectionViewWidgetModule

#endif  // TEMPORAL_PROJECTION_VIEW_WIDGET_REGISTRATION_HPP
