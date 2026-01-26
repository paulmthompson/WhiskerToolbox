#include "TableDesignerWidgetRegistration.hpp"

#include "TableDesignerWidget.hpp"
#include "TableDesignerState.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace TableDesignerWidgetModule {

void registerTypes(EditorRegistry * registry, std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "TableDesignerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    if (!data_manager) {
        std::cerr << "TableDesignerWidgetModule::registerTypes: data_manager is null" << std::endl;
        return;
    }

    // Capture data_manager for use in factory lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TableDesignerWidget"),
        .display_name = QStringLiteral("Table Designer"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Tools"),
        
        // Zone placement: TableDesigner is a pure properties widget on the right
        // It has no main view - the widget itself goes to the properties zone
        .preferred_zone = Zone::Right,
        .properties_zone = Zone::Right,  // Not used since no properties
        .prefers_split = false,
        .properties_as_tab = true,       // Add as tab in right zone
        .auto_raise_properties = false,
        
        .allow_multiple = false,         // Single instance only

        // State factory - creates the state object
        .create_state = []() {
            return std::make_shared<TableDesignerState>();
        },

        // View factory - creates the TableDesignerWidget
        .create_view = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto table_state = std::dynamic_pointer_cast<TableDesignerState>(state);
            if (!table_state) {
                std::cerr << "TableDesignerWidgetModule: Failed to cast state to TableDesignerState" << std::endl;
                return nullptr;
            }
            
            // Create the table designer widget and wire up state
            auto * widget = new TableDesignerWidget(dm);
            widget->setState(table_state);
            
            return widget;
        },

        // No properties panel - this widget IS the properties panel
        .create_properties = nullptr,

        // No custom editor creation needed
        .create_editor_custom = nullptr
    });
}

}  // namespace TableDesignerWidgetModule
