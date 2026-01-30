#ifndef EXPORT_VIDEO_WIDGET_REGISTRATION_HPP
#define EXPORT_VIDEO_WIDGET_REGISTRATION_HPP

/**
 * @file ExportVideoWidgetRegistration.hpp
 * @brief Registration function for Export_Video_Widget editor type
 * 
 * This header provides a clean interface for registering the Export_Video_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like ExportVideoWidgetState, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Export_Widgets/Export_Video_Widget/ExportVideoWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     ExportVideoWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state and view (no separate properties widget)
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow.
 * 
 * ## Zone Configuration
 * 
 * Export_Video_Widget is registered with:
 * - preferred_zone = Zone::Right (tool widget)
 * - properties_zone = Zone::Right (no separate properties)
 * - allow_multiple = false (single instance)
 * - auto_raise_properties = true (raised when opened)
 * 
 * @see EditorRegistry for the type registration API
 * @see ExportVideoWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace ExportVideoWidgetModule {

/**
 * @brief Register the Export_Video_Widget editor type with the registry
 * 
 * This function registers the ExportVideoWidget type, including:
 * - State factory: Creates ExportVideoWidgetState
 * - View factory: Creates the Export_Video_Widget (no separate properties)
 * 
 * Export_Video_Widget is a single-widget editor (no view/properties split)
 * that lives in Zone::Right as a tool widget.
 * 
 * The widget emits requestTimeChange(TimePosition) signals during export,
 * which are connected to EditorRegistry::setCurrentTime() at registration time.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}// namespace ExportVideoWidgetModule

#endif// EXPORT_VIDEO_WIDGET_REGISTRATION_HPP
