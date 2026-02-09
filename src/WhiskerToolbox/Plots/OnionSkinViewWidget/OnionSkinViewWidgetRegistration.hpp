#ifndef ONION_SKIN_VIEW_WIDGET_REGISTRATION_HPP
#define ONION_SKIN_VIEW_WIDGET_REGISTRATION_HPP

/**
 * @file OnionSkinViewWidgetRegistration.hpp
 * @brief Registration function for Onion Skin View Widget editor types
 * 
 * This header provides a clean interface for registering the Onion Skin View Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like OnionSkinViewState, OnionSkinViewWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/OnionSkinViewWidget/OnionSkinViewWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     OnionSkinViewWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see OnionSkinViewState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace OnionSkinViewWidgetModule {

/**
 * @brief Register all Onion Skin View Widget editor types with the registry
 * 
 * This function registers the OnionSkinViewWidget type, including:
 * - State factory: Creates OnionSkinViewState
 * - View factory: Creates OnionSkinViewWidget (the main plot component)
 * - Properties factory: Creates OnionSkinViewPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace OnionSkinViewWidgetModule

#endif  // ONION_SKIN_VIEW_WIDGET_REGISTRATION_HPP
