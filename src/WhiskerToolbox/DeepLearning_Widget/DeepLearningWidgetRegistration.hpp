#ifndef DEEP_LEARNING_WIDGET_REGISTRATION_HPP
#define DEEP_LEARNING_WIDGET_REGISTRATION_HPP

/**
 * @file DeepLearningWidgetRegistration.hpp
 * @brief Registration function for DeepLearning widget editor types.
 *
 * This header provides a clean interface for registering the
 * DeepLearning widget with the EditorRegistry. MainWindow calls this
 * function without needing to know implementation details like
 * DeepLearningState, DeepLearningViewWidget, etc.
 *
 * ## Usage
 *
 * ```cpp
 * #include "DeepLearning_Widget/DeepLearningWidgetRegistration.hpp"
 *
 * void MainWindow::_registerEditorTypes() {
 *     DeepLearningWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 *
 * ## Design
 *
 * The registration function encapsulates:
 * - Factory functions for state, view, and properties
 * - Type metadata (display name, menu path, zone preferences)
 * - Widget creation logic
 *
 * DeepLearning follows the View/Properties split pattern:
 * - DeepLearningViewWidget goes to Zone::Center (main visualization)
 * - DeepLearningPropertiesWidget goes to Zone::Right (persistent tab)
 *
 * @see EditorRegistry for the type registration API.
 * @see DeepLearningState for the shared state class.
 * @see DeepLearningViewWidget for the visualization component.
 * @see DeepLearningPropertiesWidget for the controls component.
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace DeepLearningWidgetModule {

/**
 * @brief Register DeepLearning widget editor types with the registry.
 *
 * This function registers the DeepLearningWidget type, including:
 * - State factory: creates DeepLearningState.
 * - View factory: creates DeepLearningViewWidget (Zone::Center).
 * - Properties factory: creates DeepLearningPropertiesWidget (Zone::Right).
 *
 * @param registry The EditorRegistry to register types with.
 * @param data_manager Shared DataManager for widget construction.
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}// namespace DeepLearningWidgetModule

#endif// DEEP_LEARNING_WIDGET_REGISTRATION_HPP
