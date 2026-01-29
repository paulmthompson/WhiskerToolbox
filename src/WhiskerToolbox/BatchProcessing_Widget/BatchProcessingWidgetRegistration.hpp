#ifndef BATCHPROCESSING_WIDGET_REGISTRATION_HPP
#define BATCHPROCESSING_WIDGET_REGISTRATION_HPP

/**
 * @file BatchProcessingWidgetRegistration.hpp
 * @brief Registration function for BatchProcessing_Widget editor type
 * 
 * This header provides a clean interface for registering the BatchProcessing_Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like BatchProcessingState.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "BatchProcessing_Widget/BatchProcessingWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     BatchProcessingWidgetModule::registerTypes(_editor_registry.get());
 * }
 * ```
 * 
 * ## Design
 * 
 * BatchProcessing_Widget is a properties-only widget for loading data from
 * multiple folders using JSON configuration. Unlike editor widgets with 
 * view/properties split, BatchProcessing is a single widget placed in 
 * Zone::Right (properties area). It has no separate view component.
 * 
 * Key characteristics:
 * - Single instance only (allow_multiple = false)
 * - Placed in Zone::Right (properties panel area)
 * - No view widget (create_view = nullptr)
 * - Uses custom factory to pass EditorRegistry to state
 * 
 * @see EditorRegistry for the type registration API
 * @see BatchProcessingState for the shared state class
 * @see BatchProcessing_Widget for the view implementation
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace BatchProcessingWidgetModule {

/**
 * @brief Register the BatchProcessing_Widget editor type with the registry
 * 
 * This function registers the BatchProcessing_Widget type, including:
 * - State factory: Creates BatchProcessingState (with EditorRegistry access)
 * - Properties factory: Creates BatchProcessing_Widget (goes to Zone::Right)
 * - No view factory (properties-only widget)
 * 
 * BatchProcessing_Widget is a single-instance utility widget.
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for data access
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace BatchProcessingWidgetModule

#endif  // BATCHPROCESSING_WIDGET_REGISTRATION_HPP
