#include "DataManagerWidgetRegistration.hpp"

#include "DataManager_Widget.hpp"
#include "DataManagerWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "TimeScrollBar/TimeScrollBar.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include <iostream>

namespace DataManagerWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   TimeScrollBar * time_scrollbar,
                   GroupManager * group_manager) {
    
    if (!registry) {
        std::cerr << "DataManagerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto ts = time_scrollbar;
    auto gm = group_manager;

    registry->registerType({
        .type_id = QStringLiteral("DataManagerWidget"),
        .display_name = QStringLiteral("Data Manager"),
        .icon_path = QStringLiteral(":/icons/data.png"),
        .menu_path = QStringLiteral("View/Data"),
        
        // Zone placement: DataManager_Widget is a navigation/selection widget in the left zone
        .preferred_zone = Zone::Left,     // Main widget goes to left zone
        .properties_zone = Zone::Left,    // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = false,   // Don't auto-raise
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<DataManagerWidgetState>();
        },

        // View factory - nullptr since we use create_editor_custom
        // DataManager_Widget needs EditorRegistry for SelectionContext access
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for complex dependencies
        // DataManager_Widget needs EditorRegistry, TimeScrollBar, and GroupManager
        .create_editor_custom = [dm, ts, gm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<DataManagerWidgetState>();

            // Create the widget with all dependencies
            auto * widget = new DataManager_Widget(dm, ts, reg, nullptr);

            // Set the group manager if available
            if (gm) {
                widget->setGroupManager(gm);
            }

            // Set explicit minimum size constraints
            widget->setMinimumSize(250, 400);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // DataManager_Widget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace DataManagerWidgetModule
