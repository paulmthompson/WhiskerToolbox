#include "BatchProcessingWidgetRegistration.hpp"

#include "BatchProcessing_Widget.hpp"
#include "BatchProcessingState.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace BatchProcessingWidgetModule {

void registerTypes(EditorRegistry * registry) {
    
    if (!registry) {
        std::cerr << "BatchProcessingWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    registry->registerType({
        .type_id = QStringLiteral("BatchProcessingWidget"),
        .display_name = QStringLiteral("Batch Processing"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Tools"),
        
        // Zone placement: BatchProcessing is a properties-only widget in the right zone
        // No view widget - it's a self-contained properties widget
        .preferred_zone = Zone::Right,       // Main widget goes to right
        .properties_zone = Zone::Right,      // Not used since no separate properties
        .prefers_split = false,
        .properties_as_tab = true,           // Add as tab in right zone
        .auto_raise_properties = false,
        
        .allow_multiple = false,             // Single instance only

        // Standard factories don't work because BatchProcessingState needs EditorRegistry*
        // Use custom factory instead
        .create_state = nullptr,
        .create_view = nullptr,
        .create_properties = nullptr,

        // Custom factory to handle EditorRegistry dependency
        .create_editor_custom = [](EditorRegistry * reg) -> EditorRegistry::EditorInstance {
            // Create state with registry access for DataManager and signals
            auto state = std::make_shared<BatchProcessingState>(reg);
            
            // Register the state
            reg->registerState(state);
            
            // Create the widget (this is a properties-only widget, so it goes in 'view' slot
            // but will be placed in the right zone based on preferred_zone)
            auto * widget = new BatchProcessing_Widget(state);
            widget->openWidget();
            
            return {
                .state = state,
                .view = widget,        // Properties-only widgets use view slot
                .properties = nullptr  // No separate properties widget
            };
        }
    });
}

}  // namespace BatchProcessingWidgetModule
