#ifndef DATA_VIEWER_WIDGET_REGISTRATION_HPP
#define DATA_VIEWER_WIDGET_REGISTRATION_HPP

/**
 * @file DataViewerWidgetRegistration.hpp
 * @brief Registration function for Data Viewer Widget editor types
 * 
 * This header provides a clean interface for registering the Data Viewer Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like DataViewerState, DataViewer_Widget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "DataViewer_Widget/DataViewerWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     DataViewerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see EditorRegistry for the type registration API
 * @see DataViewerState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace DataViewerWidgetModule {

/**
 * @brief Register all Data Viewer Widget editor types with the registry
 * 
 * This function registers the DataViewerWidget type, including:
 * - State factory: Creates DataViewerState
 * - View factory: Creates DataViewer_Widget (the OpenGL canvas/view component)
 * - Properties factory: Creates DataViewerPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace DataViewerWidgetModule

#endif  // DATA_VIEWER_WIDGET_REGISTRATION_HPP
