#ifndef TEST_WIDGET_REGISTRATION_HPP
#define TEST_WIDGET_REGISTRATION_HPP

/**
 * @file TestWidgetRegistration.hpp
 * @brief Registration function for TestWidget editor type
 * 
 * This header provides a clean interface for registering the TestWidget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like TestWidgetState, TestWidgetView, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Test_Widget/TestWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     TestWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 * 
 * ## Design Philosophy
 * 
 * The registration function encapsulates:
 * - Factory functions for state, view, and properties
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic
 * 
 * This keeps MainWindow decoupled from widget implementation details.
 * Each widget module defines its own registration, making it easy to
 * add new widget types without modifying MainWindow.
 * 
 * ## View/Properties Split Pattern
 * 
 * TestWidget demonstrates the proper View/Properties split:
 * - TestWidgetView goes to Zone::Center (the main visualization)
 * - TestWidgetProperties goes to Zone::Right (as a persistent tab)
 * - Both share the same TestWidgetState instance
 * 
 * EditorCreationController handles placing these widgets in their
 * respective zones and connecting cleanup signals.
 * 
 * @see EditorRegistry for the type registration API
 * @see TestWidgetState for the shared state class
 * @see TestWidgetView for the visualization component
 * @see TestWidgetProperties for the controls component
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace TestWidgetModule {

/**
 * @brief Register the TestWidget editor type with the registry
 * 
 * This function registers the TestWidget type, including:
 * - State factory: Creates TestWidgetState
 * - View factory: Creates TestWidgetView (goes to Zone::Center)
 * - Properties factory: Creates TestWidgetProperties (goes to Zone::Right)
 * 
 * TestWidget is a single-instance editor demonstrating the View/Properties
 * split pattern where view and properties are placed in different zones.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace TestWidgetModule

#endif  // TEST_WIDGET_REGISTRATION_HPP
