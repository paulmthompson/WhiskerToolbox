#include "TerminalWidgetRegistration.hpp"

#include "TerminalWidget.hpp"
#include "TerminalWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace TerminalWidgetModule {

void registerTypes(EditorRegistry * registry) {
    
    if (!registry) {
        std::cerr << "TerminalWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    registry->registerType({
        .type_id = QStringLiteral("TerminalWidget"),
        .display_name = QStringLiteral("Terminal"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Tools"),
        
        // Zone placement: Terminal is a utility widget at the right
        // No properties panel - it's a self-contained widget
        .preferred_zone = Zone::Right,
        .properties_zone = Zone::Right,  // Not used since no properties
        .prefers_split = false,
        .properties_as_tab = false,
        .auto_raise_properties = false,
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the state object
        .create_state = []() {
            return std::make_shared<TerminalWidgetState>();
        },

        // View factory - creates the TerminalWidget view
        // Note: TerminalWidget does not currently take state in constructor,
        // so we create it without state dependency. The state can be connected
        // externally if needed for preferences.
        .create_view = [](std::shared_ptr<EditorState> state) -> QWidget * {
            auto terminal_state = std::dynamic_pointer_cast<TerminalWidgetState>(state);
            if (!terminal_state) {
                std::cerr << "TerminalWidgetModule: Failed to cast state to TerminalWidgetState" << std::endl;
                return nullptr;
            }
            
            // Create the terminal widget
            auto * terminal = new TerminalWidget();
            
            // Initialize stream redirection (equivalent to openWidget())
            terminal->openWidget();
            
            return terminal;
        },

        // No properties panel for Terminal widget
        .create_properties = nullptr,

        // No custom editor creation needed
        .create_editor_custom = nullptr
    });
}

}  // namespace TerminalWidgetModule
