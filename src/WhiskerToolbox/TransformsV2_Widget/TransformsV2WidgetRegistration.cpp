#include "TransformsV2WidgetRegistration.hpp"

#include "Core/TransformsV2State.hpp"
#include "UI/TransformsV2Properties_Widget.hpp"

#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace TransformsV2WidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    if (!registry) {
        return;
    }
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TransformsV2Widget"),
        .display_name = QStringLiteral("Transforms V2"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Tools"),

        .preferred_zone = Zone::Right,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = false,
        .allow_multiple = false,

        // State factory
        .create_state = [dm]() {
            return std::make_shared<TransformsV2State>(dm);
        },

        // View factory — nullptr, we use create_editor_custom
        .create_view = nullptr,

        // Properties factory — nullptr, we use create_editor_custom
        .create_properties = nullptr,

        // Custom editor creation for SelectionContext access
        .create_editor_custom = [dm](EditorRegistry * reg)
            -> EditorRegistry::EditorInstance
        {
            auto state = std::make_shared<TransformsV2State>(dm);

            auto * selection_context = reg->selectionContext();

            auto * widget = new TransformsV2Properties_Widget(state, selection_context, nullptr);
            widget->setMinimumSize(350, 400);
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

} // namespace TransformsV2WidgetModule
