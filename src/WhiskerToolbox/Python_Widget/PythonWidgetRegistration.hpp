#ifndef PYTHON_WIDGET_REGISTRATION_HPP
#define PYTHON_WIDGET_REGISTRATION_HPP

/**
 * @file PythonWidgetRegistration.hpp
 * @brief Registration function for Python Widget editor types
 *
 * Provides a clean interface for registering the Python Widget
 * with the EditorRegistry. MainWindow calls this function without
 * needing to know implementation details.
 *
 * @see EditorRegistry for the type registration API
 * @see PythonWidgetState for the shared state class
 */

#include <memory>

class EditorRegistry;
class DataManager;

namespace PythonWidgetModule {

/**
 * @brief Register Python Widget editor type with the registry
 *
 * Registers the PythonWidget type, including:
 * - State factory: Creates PythonWidgetState
 * - View factory: Creates PythonViewWidget
 * - Properties factory: Creates PythonPropertiesWidget
 *
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace PythonWidgetModule

#endif  // PYTHON_WIDGET_REGISTRATION_HPP
