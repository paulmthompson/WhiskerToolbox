/**
 * @file CommandLogWidgetRegistration.hpp
 * @brief Registration function for CommandLog editor type
 *
 * Provides a clean interface for registering the CommandLog widget
 * with the EditorRegistry. MainWindow calls this function without
 * needing to know implementation details.
 *
 * ## Usage
 *
 * ```cpp
 * #include "CommandLog_Widget/CommandLogWidgetRegistration.hpp"
 *
 * void MainWindow::_registerEditorTypes() {
 *     CommandLogWidgetModule::registerTypes(_editor_registry.get(), commandRecorder());
 * }
 * ```
 *
 * @see CommandLogWidget for the view implementation
 * @see CommandLogState for the state class
 */

#ifndef COMMAND_LOG_WIDGET_REGISTRATION_HPP
#define COMMAND_LOG_WIDGET_REGISTRATION_HPP

class EditorRegistry;

namespace commands {
class CommandRecorder;
}// namespace commands

namespace CommandLogWidgetModule {

/**
 * @brief Register the CommandLog editor type with the registry
 *
 * Registers a single-panel widget (view only, no properties) with:
 * - View panel: CommandLogWidget (Zone::Right)
 * - Single instance only
 * - Menu path: View/Tools
 *
 * @param registry The EditorRegistry to register types with
 * @param recorder Non-owning pointer to the shared CommandRecorder
 */
void registerTypes(EditorRegistry * registry,
                   commands::CommandRecorder * recorder);

}// namespace CommandLogWidgetModule

#endif// COMMAND_LOG_WIDGET_REGISTRATION_HPP
