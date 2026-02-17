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

        .create_state = [dm]() {
            return std::make_shared<TransformsV2State>(dm);
        },

        .create_view = nullptr,

        .create_properties = [](std::shared_ptr<EditorState> state) -> QWidget * {
            auto tv2_state = std::dynamic_pointer_cast<TransformsV2State>(state);
            if (!tv2_state) {
                return nullptr;
            }
            auto * widget = new TransformsV2Properties_Widget(tv2_state);
            widget->setMinimumSize(350, 400);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
            return widget;
        },

        .create_editor_custom = nullptr
    });
}

} // namespace TransformsV2WidgetModule
