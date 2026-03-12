/**
 * @file DataSynthesizerWidgetRegistration.hpp
 * @brief Registration function for DataSynthesizer editor type
 *
 * Provides a clean interface for registering the DataSynthesizer widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like DataSynthesizerState.
 *
 * ## Usage
 *
 * ```cpp
 * #include "DataSynthesizer_Widget/DataSynthesizerWidgetRegistration.hpp"
 *
 * void MainWindow::_registerEditorTypes() {
 *     DataSynthesizerWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
 * }
 * ```
 *
 * @see EditorRegistry for the type registration API
 * @see DataSynthesizerState for the shared state class
 */

#ifndef DATA_SYNTHESIZER_WIDGET_REGISTRATION_HPP
#define DATA_SYNTHESIZER_WIDGET_REGISTRATION_HPP

#include <memory>

class DataManager;
class EditorRegistry;

namespace DataSynthesizerWidgetModule {

/**
 * @brief Register the DataSynthesizer editor type with the registry
 *
 * Registers a dual-panel widget (view + properties) with:
 * - View panel: DataSynthesizerView_Widget (Zone::Right)
 * - Properties panel: DataSynthesizerProperties_Widget (Zone::Right, as tab)
 * - Single instance only
 * - Menu path: View/Tools
 *
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager instance
 */
void registerTypes(EditorRegistry * registry, std::shared_ptr<DataManager> const & data_manager);

}// namespace DataSynthesizerWidgetModule

#endif// DATA_SYNTHESIZER_WIDGET_REGISTRATION_HPP
