#ifndef MLCORE_WIDGET_REGISTRATION_HPP
#define MLCORE_WIDGET_REGISTRATION_HPP

/**
 * @file MLCoreWidgetRegistration.hpp
 * @brief Registration function for MLCoreWidget editor type
 *
 * Provides a clean interface for registering the MLCore_Widget with the
 * EditorRegistry. MainWindow calls this function without needing to know
 * implementation details like MLCoreWidgetState or MLCoreWidget.
 *
 * ## Usage
 *
 * ```cpp
 * #include "MLCore_Widget/MLCoreWidgetRegistration.hpp"
 *
 * void MainWindow::_registerEditorTypes() {
 *     MLCoreWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 *
 * ## Zone Placement
 *
 * MLCoreWidget is configured for:
 * - preferred_zone = Zone::Right (tool panel in the right dock zone)
 * - allow_multiple = false (single instance only)
 * - properties_as_tab = true (add as tab in the zone)
 *
 * ## Menu Location
 *
 * The widget appears under View/Analysis in the application menu, alongside
 * other analysis widgets (scatter plot, event plot, etc.).
 *
 * @see MLCoreWidget for the main widget implementation
 * @see MLCoreWidgetState for state management
 */

#include <memory>

// Forward declarations
class DataManager;
class EditorRegistry;

namespace MLCoreWidgetModule {

/**
 * @brief Register the MLCoreWidget editor type with the registry
 *
 * Registers the MLCoreWidget type including:
 * - State factory: Creates MLCoreWidgetState
 * - View factory: Creates MLCoreWidget (goes to Zone::Right)
 * - No properties factory (self-contained tool widget pattern)
 *
 * Uses create_editor_custom to get EditorRegistry access for
 * SelectionContext integration.
 *
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace MLCoreWidgetModule

#endif  // MLCORE_WIDGET_REGISTRATION_HPP
