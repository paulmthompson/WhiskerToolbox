/**
 * @file CommandLogWidgetRegistration.cpp
 * @brief Registration implementation for the CommandLog editor type
 */

#include "CommandLogWidgetRegistration.hpp"

#include "Core/CommandLogState.hpp"
#include "UI/CommandLogWidget.hpp"
#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace CommandLogWidgetModule {

void registerTypes(EditorRegistry * registry,
                   commands::CommandRecorder * recorder) {
    if (!registry) {
        std::cerr << "CommandLogWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    auto cr = recorder;

    registry->registerType({.type_id = QStringLiteral("CommandLogWidget"),
                            .display_name = QStringLiteral("Command Log"),
                            .icon_path = QString{},
                            .menu_path = QStringLiteral("View/Tools"),

                            .preferred_zone = Zone::Right,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = false,
                            .auto_raise_properties = false,

                            .allow_multiple = false,

                            .create_state = []() { return std::make_shared<CommandLogState>(); },

                            .create_view = [cr](const std::shared_ptr<EditorState>& /*state*/) -> QWidget * {
                                return new CommandLogWidget(cr);
                            },

                            .create_properties = nullptr,
                            .create_editor_custom = nullptr});
}

}// namespace CommandLogWidgetModule
