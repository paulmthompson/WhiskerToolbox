#include "DataImportWidgetRegistration.hpp"

#include "DataImport_Widget.hpp"
#include "DataImportWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace DataImportWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "DataImportWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("DataImportWidget"),
        .display_name = QStringLiteral("Data Import"),
        .icon_path = QString{},  // TODO: Add import icon
        .menu_path = QStringLiteral("View/Tools"),
        
        // Zone placement: DataImport_Widget is a properties panel in the right zone
        // It responds to data focus changes via passive awareness pattern
        .preferred_zone = Zone::Right,    // Main widget goes to right zone
        .properties_zone = Zone::Right,   // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = false,   // Don't auto-raise (user controls visibility)
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<DataImportWidgetState>();
        },

        // View factory - nullptr because we use create_editor_custom
        // (DataImport_Widget needs EditorRegistry for SelectionContext access)
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for EditorRegistry access
        // DataImport_Widget needs EditorRegistry to access SelectionContext
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<DataImportWidgetState>();

            // Get SelectionContext from the registry
            auto * selection_context = reg->selectionContext();

            // Create the widget with state, DataManager, and SelectionContext
            auto * widget = new DataImport_Widget(state, dm, selection_context, nullptr);

            // Set size constraints suitable for right zone
            // DataImport_Widget needs enough space for file dialogs and options
            widget->setMinimumSize(300, 400);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            // Register the state
            reg->registerState(state);

            // DataImport_Widget is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace DataImportWidgetModule
