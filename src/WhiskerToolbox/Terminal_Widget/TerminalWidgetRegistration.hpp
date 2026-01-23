#ifndef TERMINAL_WIDGET_REGISTRATION_HPP
#define TERMINAL_WIDGET_REGISTRATION_HPP

/**
 * @file TerminalWidgetRegistration.hpp
 * @brief Registration function for TerminalWidget editor type
 * 
 * This header provides a clean interface for registering the TerminalWidget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TerminalWidgetState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Terminal_Widget/TerminalWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TerminalWidgetModule::registerTypes(_editor_registry.get());
 * }
 * ```
 * 
 * ## Design
 * 
 * TerminalWidget is a utility widget that captures stdout/stderr. Unlike
 * editor widgets with view/properties split, Terminal is a single widget
 * placed in Zone::Bottom. It serves as both the view and has no separate
 * properties panel.
 * 
 * Key characteristics:
 * - Single instance only (allow_multiple = false)
 * - Placed in Zone::Bottom (below the main editor area)
 * - No properties panel (create_properties = nullptr)
 * 
 * @see EditorRegistry for the type registration API
 * @see TerminalWidgetState for the shared state class
 * @see TerminalWidget for the view implementation
 */

class EditorRegistry;

namespace TerminalWidgetModule {

/**
 * @brief Register the TerminalWidget editor type with the registry
 * 
 * This function registers the TerminalWidget type, including:
 * - State factory: Creates TerminalWidgetState
 * - View factory: Creates TerminalWidget (goes to Zone::Bottom)
 * - No properties factory (single-panel widget)
 * 
 * TerminalWidget is a single-instance utility widget.
 * 
 * @param registry The EditorRegistry to register types with
 */
void registerTypes(EditorRegistry * registry);

}  // namespace TerminalWidgetModule

#endif  // TERMINAL_WIDGET_REGISTRATION_HPP
