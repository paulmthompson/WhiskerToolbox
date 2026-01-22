#include "DataTransformWidgetRegistration.hpp"

#include "DataTransform_Widget.hpp"
#include "DataTransformWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace DataTransformWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "DataTransformWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("DataTransformWidget"),
        .display_name = QStringLiteral("Data Transforms"),
        .icon_path = QStringLiteral(":/icons/transform.png"),
        .menu_path = QStringLiteral("View/Tools"),
        
        // Zone placement: DataTransformWidget is a tool widget in the right zone
        // It has no separate "view" - the widget itself is the tool
        .preferred_zone = Zone::Right,    // Main widget goes to right zone
        .properties_zone = Zone::Right,   // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = false,   // Don't auto-raise
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<DataTransformWidgetState>();
        },

        // View factory - creates the DataTransform_Widget
        // Note: The widget requires EditorRegistry for SelectionContext access,
        // so we use create_editor_custom instead
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for EditorRegistry access
        // DataTransform_Widget needs EditorRegistry to access SelectionContext
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<DataTransformWidgetState>();

            // Create the widget with EditorRegistry for SelectionContext access
            auto * widget = new DataTransform_Widget(dm, reg, nullptr);

            // Set explicit minimum size constraints
            widget->setMinimumSize(350, 700);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // DataTransform_Widget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace DataTransformWidgetModule
