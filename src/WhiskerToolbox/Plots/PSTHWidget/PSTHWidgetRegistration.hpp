#ifndef PSTH_WIDGET_REGISTRATION_HPP
#define PSTH_WIDGET_REGISTRATION_HPP

/**
 * @file PSTHWidgetRegistration.hpp
 * @brief Registration function for PSTH Widget editor types
 * 
 * This header provides a clean interface for registering the PSTH Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like PSTHState, PSTHWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/PSTHWidget/PSTHWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     PSTHWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see PSTHState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace PSTHWidgetModule {

/**
 * @brief Register all PSTH Widget editor types with the registry
 * 
 * This function registers the PSTHWidget type, including:
 * - State factory: Creates PSTHState
 * - View factory: Creates PSTHWidget (the main plot component)
 * - Properties factory: Creates PSTHPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace PSTHWidgetModule

#endif  // PSTH_WIDGET_REGISTRATION_HPP
