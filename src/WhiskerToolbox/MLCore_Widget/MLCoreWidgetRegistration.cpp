#include "MLCoreWidgetRegistration.hpp"

#include "MLCoreWidget.hpp"
#include "MLCoreWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace MLCoreWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "MLCoreWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    auto dm = std::move(data_manager);

    registry->registerType({
        .type_id = QStringLiteral("MLCoreWidget"),
        .display_name = QStringLiteral("ML Workflow"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Analysis"),

        // Zone placement: single-instance tool panel in the right zone
        .preferred_zone = Zone::Right,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = false,

        .allow_multiple = false,

        .create_state = []() {
            return std::make_shared<MLCoreWidgetState>();
        },

        // Use create_editor_custom for EditorRegistry access (SelectionContext)
        .create_view = nullptr,
        .create_properties = nullptr,

        .create_editor_custom = [dm](EditorRegistry * reg)
            -> EditorRegistry::EditorInstance
        {
            auto state = std::make_shared<MLCoreWidgetState>();

            auto * selection_context = reg->selectionContext();

            auto * widget = new MLCoreWidget(state, dm, selection_context, nullptr);

            widget->setMinimumSize(350, 500);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

            reg->registerState(state);

            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace MLCoreWidgetModule
