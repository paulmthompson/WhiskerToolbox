#include "GroupManagementWidgetRegistration.hpp"

#include "GroupManagementWidget.hpp"
#include "GroupManagementWidgetState.hpp"
#include "GroupManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace GroupManagementWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager) {
    
    if (!registry) {
        std::cerr << "GroupManagementWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    if (!group_manager) {
        std::cerr << "GroupManagementWidgetModule::registerTypes: group_manager is null, skipping registration" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto gm = group_manager;

    registry->registerType({
        .type_id = QStringLiteral("GroupManagementWidget"),
        .display_name = QStringLiteral("Group Manager"),
        .icon_path = QStringLiteral(":/icons/groups.png"),
        .menu_path = QStringLiteral("View/Data"),
        
        // Zone placement: GroupManagementWidget is a navigation widget in the left zone
        // It sits above the DataManager_Widget
        .preferred_zone = Zone::Left,     // Main widget goes to left zone
        .properties_zone = Zone::Left,    // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = false,   // Don't auto-raise
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<GroupManagementWidgetState>();
        },

        // View factory - nullptr since we use create_editor_custom
        // GroupManagementWidget needs GroupManager for construction
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for GroupManager dependency
        .create_editor_custom = [dm, gm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<GroupManagementWidgetState>();

            // Create the widget with GroupManager
            auto * widget = new GroupManagementWidget(gm, nullptr);

            // Set size constraints - this is a compact panel
            widget->setMinimumSize(200, 150);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // GroupManagementWidget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace GroupManagementWidgetModule
