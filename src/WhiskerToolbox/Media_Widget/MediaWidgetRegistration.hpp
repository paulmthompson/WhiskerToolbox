#ifndef MEDIA_WIDGET_REGISTRATION_HPP
#define MEDIA_WIDGET_REGISTRATION_HPP

/**
 * @file MediaWidgetRegistration.hpp
 * @brief Registration function for Media Widget editor types
 * 
 * This header provides a clean interface for registering the Media Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like MediaWidgetState, MediaViewWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Media_Widget/MediaWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     MediaWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state, view, and properties
 * - Type metadata (display name, menu path, default zone)
 * - Complex widget creation logic (e.g., view/properties coupling)
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow.
 * 
 * @see EditorRegistry for the type registration API
 * @see MediaWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;
class GroupManager;

namespace MediaWidgetModule {

/**
 * @brief Register all Media Widget editor types with the registry
 * 
 * This function registers the MediaWidget type, including:
 * - State factory: Creates MediaWidgetState
 * - View factory: Creates the view widget (future: MediaViewWidget)
 * - Properties factory: Creates the properties widget (future: MediaPropertiesWidget)
 * 
 * Currently, the Media Widget uses a custom factory function because
 * the properties widget needs a reference to Media_Window owned by the view.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 * @param group_manager Optional GroupManager for group-aware features (can be nullptr)
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager = nullptr);

}  // namespace MediaWidgetModule

#endif  // MEDIA_WIDGET_REGISTRATION_HPP
