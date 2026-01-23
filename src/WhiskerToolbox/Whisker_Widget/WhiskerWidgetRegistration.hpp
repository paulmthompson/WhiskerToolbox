#ifndef WHISKER_WIDGET_REGISTRATION_HPP
#define WHISKER_WIDGET_REGISTRATION_HPP

/**
 * @file WhiskerWidgetRegistration.hpp
 * @brief Registration function for Whisker Widget editor types
 * 
 * This header provides a clean interface for registering the Whisker Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like WhiskerWidgetState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Whisker_Widget/WhiskerWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     WhiskerWidgetModule::registerTypes(_editor_registry.get(), _data_manager, _time_scrollbar);
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
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow.
 * 
 * ## Zone Placement
 * 
 * WhiskerWidget is a tool widget that goes to the Right zone.
 * It is a "pure properties" widget - there is no separate view/properties split.
 * The entire widget is placed in Zone::Right as a tab alongside other tool widgets.
 * 
 * @see EditorRegistry for the type registration API
 * @see WhiskerWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;
class TimeScrollBar;

namespace WhiskerWidgetModule {

/**
 * @brief Register Whisker Widget editor types with the registry
 * 
 * This function registers the WhiskerWidget type, including:
 * - State factory: Creates WhiskerWidgetState
 * - View factory: Creates Whisker_Widget (combined view - no view/properties split)
 * - Properties factory: nullptr (widget is self-contained)
 * 
 * @param registry The EditorRegistry to register with
 * @param data_manager Shared DataManager for data access
 * @param time_scrollbar TimeScrollBar for frame change notifications
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   TimeScrollBar * time_scrollbar);

}  // namespace WhiskerWidgetModule

#endif // WHISKER_WIDGET_REGISTRATION_HPP
