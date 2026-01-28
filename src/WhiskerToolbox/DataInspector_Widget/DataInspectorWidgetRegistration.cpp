#include "DataInspectorWidgetRegistration.hpp"

#include "DataInspectorState.hpp"
#include "DataInspectorViewWidget.hpp"
#include "DataInspectorPropertiesWidget.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include <iostream>

namespace DataInspectorModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager) {
    
    if (!registry) {
        std::cerr << "DataInspectorModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto gm = group_manager;

    registry->registerType({
        .type_id = QStringLiteral("DataInspector"),
        .display_name = QStringLiteral("Data Inspector"),
        .icon_path = QStringLiteral(":/icons/inspect.png"),
        .menu_path = QStringLiteral("View/Data"),
        .preferred_zone = Zone::Center,       // View widget goes to center
        .properties_zone = Zone::Right,       // Properties widget goes to right
        .prefers_split = false,
        .properties_as_tab = true,            // Add as tab, don't replace
        .auto_raise_properties = false,       // Don't obscure other tools
        .allow_multiple = true,               // Multiple inspectors allowed

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<DataInspectorState>();
        },

        // View factory - creates DataInspectorViewWidget (Center zone)
        .create_view = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto inspector_state = std::dynamic_pointer_cast<DataInspectorState>(state);
            if (!inspector_state) {
                std::cerr << "DataInspectorModule: Failed to cast state to DataInspectorState" << std::endl;
                return nullptr;
            }

            auto * widget = new DataInspectorViewWidget(dm);
            widget->setState(inspector_state);
            return widget;
        },

        // Properties factory - creates DataInspectorPropertiesWidget (Right zone)
        .create_properties = [dm, gm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto inspector_state = std::dynamic_pointer_cast<DataInspectorState>(state);
            if (!inspector_state) {
                std::cerr << "DataInspectorModule: Failed to cast state to DataInspectorState for properties" << std::endl;
                return nullptr;
            }

            auto * widget = new DataInspectorPropertiesWidget(dm, gm);
            widget->setState(inspector_state);
            return widget;
        },

        // Custom editor creation for complex view/properties coupling
        // Ensures both widgets share the same state and SelectionContext
        .create_editor_custom = [dm, gm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<DataInspectorState>();

            // Create the view widget (Center zone)
            auto * view = new DataInspectorViewWidget(dm);
            view->setState(state);

            // Create the properties widget (Right zone)
            auto * props = new DataInspectorPropertiesWidget(dm, gm);
            props->setState(state);

            // Connect properties to selection context from registry
            if (reg) {
                props->setSelectionContext(reg->selectionContext());
            }

            // Connect frame selection signals
            QObject::connect(props, &DataInspectorPropertiesWidget::frameSelected,
                             view, &DataInspectorViewWidget::frameSelected);
            QObject::connect(view, &DataInspectorViewWidget::frameSelected,
                             props, &DataInspectorPropertiesWidget::frameSelected);

            // Register the state
            reg->registerState(state);

            return EditorRegistry::EditorInstance{
                .state = state,
                .view = view,
                .properties = props
            };
        }
    });
}

}  // namespace DataInspectorModule
