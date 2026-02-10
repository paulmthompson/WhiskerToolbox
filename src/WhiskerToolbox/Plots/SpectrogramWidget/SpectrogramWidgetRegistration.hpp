#ifndef SPECTROGRAM_WIDGET_REGISTRATION_HPP
#define SPECTROGRAM_WIDGET_REGISTRATION_HPP

/**
 * @file SpectrogramWidgetRegistration.hpp
 * @brief Registration function for Spectrogram Widget editor types
 * 
 * This header provides a clean interface for registering the Spectrogram Widget
 * with the EditorRegistry. MainWindow calls this function without needing
 * to know implementation details like SpectrogramState, SpectrogramWidget, etc.
 * 
 * ## Usage
 * 
 * ```cpp
 * #include "Plots/SpectrogramWidget/SpectrogramWidgetRegistration.hpp"
 * 
 * void MainWindow::_registerEditorTypes() {
 *     SpectrogramWidgetModule::registerTypes(_editor_registry.get(), _data_manager);
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
 * @see SpectrogramState for the shared state class
 */

#include <memory>

class DataManager;
class EditorRegistry;

namespace SpectrogramWidgetModule {

/**
 * @brief Register all Spectrogram Widget editor types with the registry
 * 
 * This function registers the SpectrogramWidget type, including:
 * - State factory: Creates SpectrogramState
 * - View factory: Creates SpectrogramWidget (the main plot component)
 * - Properties factory: Creates SpectrogramPropertiesWidget
 * 
 * @param registry The EditorRegistry to register types with
 * @param data_manager Shared DataManager for widget construction
 */
void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager);

}  // namespace SpectrogramWidgetModule

#endif  // SPECTROGRAM_WIDGET_REGISTRATION_HPP
