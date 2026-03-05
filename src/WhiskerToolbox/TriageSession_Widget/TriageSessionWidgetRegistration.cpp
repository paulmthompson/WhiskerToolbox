/**
 * @file TriageSessionWidgetRegistration.cpp
 * @brief Registration of the TriageSession widget with the EditorRegistry
 */

#include "TriageSessionWidgetRegistration.hpp"

#include <utility>

#include "Core/TriageSessionState.hpp"
#include "UI/TriageSessionProperties_Widget.hpp"

#include "EditorState/EditorRegistry.hpp"

namespace TriageSessionWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    if (!registry) {
        return;
    }
    auto dm = data_manager;

    registry->registerType({
            .type_id = QStringLiteral("TriageSessionWidget"),
            .display_name = QStringLiteral("Triage Session"),
            .icon_path = QString{},
            .menu_path = QStringLiteral("View/Tools"),

            .preferred_zone = Zone::Right,
            .properties_zone = Zone::Right,
            .prefers_split = false,
            .properties_as_tab = true,
            .auto_raise_properties = false,
            .allow_multiple = false,

            // State factory
            .create_state = [dm]() { return std::make_shared<TriageSessionState>(dm); },

            // No view — properties-only widget
            .create_view = nullptr,

            // Properties factory
            .create_properties = [](std::shared_ptr<EditorState> const & state) -> QWidget * {
                auto ts_state = std::dynamic_pointer_cast<TriageSessionState>(state);
                if (!ts_state) {
                    return nullptr;
                }
                auto * widget = new TriageSessionProperties_Widget(std::move(ts_state));
                widget->setMinimumSize(300, 200);
                return widget;
            },

            // No custom editor — using standard create_state + create_properties
            .create_editor_custom = nullptr,
    });
}

}// namespace TriageSessionWidgetModule
