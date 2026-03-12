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
                   const std::shared_ptr<DataManager>& data_manager,
                   commands::CommandRecorder * recorder) {
    if (!registry) {
        return;
    }
    auto const & dm = std::move(data_manager);
    auto cr = recorder;

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

            // Properties factory — not used (create_editor_custom provides EditorRegistry)
            .create_properties = nullptr,

            // Custom editor: provides EditorRegistry for time tracking
            .create_editor_custom = [dm, cr](EditorRegistry * reg)
                    -> EditorRegistry::EditorInstance {
                auto state = std::make_shared<TriageSessionState>(dm);

                auto * widget = new TriageSessionProperties_Widget(state, reg);
                widget->setCommandRecorder(cr);
                widget->setMinimumSize(300, 200);

                reg->registerState(state);

                return EditorRegistry::EditorInstance{
                        .state = state,
                        .view = widget,
                        .properties = nullptr};
            },
    });
}

}// namespace TriageSessionWidgetModule
