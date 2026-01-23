#ifndef TIMESCROLLBAR_REGISTRATION_HPP
#define TIMESCROLLBAR_REGISTRATION_HPP

/**
 * @file TimeScrollBarRegistration.hpp
 * @brief Registration function for TimeScrollBar editor types
 * 
 * This header provides a clean interface for registering the TimeScrollBar
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TimeScrollBarState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "TimeScrollBar/TimeScrollBarRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TimeScrollBarModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * ## Zone Placement
 * 
 * TimeScrollBar is the global timeline widget that goes to Zone::Bottom.
 * It is a singleton widget - only one instance should exist.
 * 
 * @see EditorRegistry for the type registration API
 * @see TimeScrollBarState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace TimeScrollBarModule {

/**
 * @brief Register TimeScrollBar editor types with the registry
 * 
 * This function registers the TimeScrollBar type, including:
 * - State factory: Creates TimeScrollBarState
 * - View factory: Creates TimeScrollBar (combined view - no view/properties split)
 * - Properties factory: nullptr (widget is self-contained)
 * 
 * @param registry The EditorRegistry to register with
 * @param data_manager Shared DataManager for data access
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace TimeScrollBarModule

#endif // TIMESCROLLBAR_REGISTRATION_HPP
