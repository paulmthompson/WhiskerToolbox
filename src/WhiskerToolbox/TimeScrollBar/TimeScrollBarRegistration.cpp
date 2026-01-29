#include "TimeScrollBarRegistration.hpp"

#include "TimeScrollBar.hpp"
#include "TimeScrollBarState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace TimeScrollBarModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "TimeScrollBarModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TimeScrollBar"),
        .display_name = QStringLiteral("Timeline"),
        .icon_path = QStringLiteral(":/icons/timeline.png"),
        .menu_path = QStringLiteral("View/Timeline"),
        
        // Zone placement: TimeScrollBar goes to bottom zone
        // It is the global timeline control widget
        .preferred_zone = Zone::Bottom,   // Main widget goes to bottom zone
        .properties_zone = Zone::Bottom,  // No separate properties
        .prefers_split = false,
        .properties_as_tab = false,       // Not a tab - fills the zone
        .auto_raise_properties = false,   // No properties to raise
        
        .allow_multiple = false,          // Single instance only (global timeline)

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<TimeScrollBarState>();
        },

        // View factory - nullptr since we use custom editor creation
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for DataManager access
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<TimeScrollBarState>();

            // Create the widget with DataManager and state
            auto * widget = new TimeScrollBar(dm, state, nullptr);

            // Set EditorRegistry for time synchronization
            widget->setEditorRegistry(reg);

            // Initialize TimeFrame from DataManager (default to "time" TimeKey)
            if (dm) {
                TimeKey default_key("time");
                auto tf = dm->getTime(default_key);
                if (tf) {
                    widget->setTimeFrame(tf, default_key);
                } else {
                    // Fallback: try to get any available TimeFrame
                    auto keys = dm->getTimeFrameKeys();
                    if (!keys.empty()) {
                        auto fallback_tf = dm->getTime(keys[0]);
                        if (fallback_tf) {
                            widget->setTimeFrame(fallback_tf, keys[0]);
                        }
                    }
                }
            }

            // Register the state
            reg->registerState(state);

            // TimeScrollBar is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace TimeScrollBarModule
