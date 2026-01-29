#include "WhiskerWidgetRegistration.hpp"

#include "Whisker_Widget.hpp"
#include "WhiskerWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace WhiskerWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "WhiskerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("WhiskerWidget"),
        .display_name = QStringLiteral("Whisker Tracking"),
        .icon_path = QStringLiteral(":/icons/whisker.png"),
        .menu_path = QStringLiteral("Analysis/Whisker"),
        
        // Zone placement: WhiskerWidget is a tool widget in the right zone
        // It has no separate "view" - the widget itself is the tool
        .preferred_zone = Zone::Right,    // Main widget goes to right zone
        .properties_zone = Zone::Right,   // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = true,    // Auto-raise when opened
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<WhiskerWidgetState>();
        },

        // View factory - nullptr since we use custom editor creation
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for DataManager and TimeScrollBar access
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<WhiskerWidgetState>();

            // Create the widget with DataManager
            auto * widget = new Whisker_Widget(dm, state, nullptr);

            // Connect EditorRegistry time changes to widget
            // Use the new timeChanged(TimePosition) signal (preferred)
            QObject::connect(reg,
                             QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                             widget, QOverload<TimePosition>::of(&Whisker_Widget::LoadFrame));

            // Set explicit minimum size constraints for proper docking
            widget->setMinimumSize(400, 500);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // Initialize the widget (calls openWidget internally handled setup)
            widget->openWidget();

            // Whisker_Widget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace WhiskerWidgetModule
