/**
 * @file KeybindingEditorRegistration.cpp
 * @brief Registers the KeybindingEditor widget type with the EditorRegistry
 */

#include "KeybindingEditorRegistration.hpp"

#include "KeybindingEditor.hpp"
#include "KeybindingEditorState.hpp"

#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace KeybindingEditorModule {

namespace {

// No editor actions or context actions for this widget —
// it is the UI for managing those registrations, not a consumer of them.

}// namespace

void registerTypes(EditorRegistry * registry, KeymapSystem::KeymapManager * keymap_manager) {
    if (!registry) {
        std::cerr << "KeybindingEditorModule::registerTypes: registry is null\n";
        return;
    }

    auto * km = keymap_manager;

    registry->registerType({.type_id = QStringLiteral("KeybindingEditor"),
                            .display_name = QStringLiteral("Keybinding Editor"),
                            .icon_path = QString{},
                            .menu_path = QStringLiteral("View/Tools"),

                            .preferred_zone = Zone::Right,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = false,
                            .auto_raise_properties = false,

                            .allow_multiple = false,

                            .create_state = nullptr,
                            .create_view = nullptr,
                            .create_properties = nullptr,

                            .create_editor_custom = [km](EditorRegistry * reg) -> EditorRegistry::EditorInstance {
                                auto state = std::make_shared<KeybindingEditorState>();
                                auto * view = new KeybindingEditor(state, km);
                                reg->registerState(state);
                                return EditorRegistry::EditorInstance{.state = state, .view = view, .properties = nullptr};
                            }});
}

}// namespace KeybindingEditorModule
