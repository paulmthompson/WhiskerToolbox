#include "TongueWidgetRegistration.hpp"

#include "Tongue_Widget.hpp"
#include "TongueWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace TongueWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "TongueWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TongueWidget"),
        .display_name = QStringLiteral("Tongue Tracking"),
        .icon_path = QStringLiteral(":/icons/tongue.png"),
        .menu_path = QStringLiteral("Analysis/Tongue"),
        
        // Zone placement: TongueWidget is a tool widget in the right zone
        // It has no separate "view" - the widget itself is the tool
        .preferred_zone = Zone::Right,    // Main widget goes to right zone
        .properties_zone = Zone::Right,   // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = true,    // Auto-raise when opened
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<TongueWidgetState>();
        },

        // View factory - nullptr since we use custom editor creation
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for EditorRegistry access
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<TongueWidgetState>();

            // Create the widget
            auto * widget = new Tongue_Widget(dm, state, nullptr);

            // Set explicit minimum size constraints
            widget->setMinimumSize(350, 400);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // Tongue_Widget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace TongueWidgetModule
