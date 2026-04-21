/**
 * @file TriageSessionWidgetRegistration.cpp
 * @brief Registration of the TriageSession widget with the EditorRegistry
 */

#include "TriageSessionWidgetRegistration.hpp"

#include "Core/TriageSessionState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "UI/TriageSessionProperties_Widget.hpp"

#include <QKeySequence>
#include <utility>

namespace TriageSessionWidgetModule {

namespace {

void registerSequenceSlotActions(KeymapSystem::KeymapManager * km,
                                 KeymapSystem::KeyActionScope const & scope,
                                 QString const & category) {
    if (!km) {
        return;
    }

    for (int slot_index = 1; slot_index <= TriageSessionState::sequenceSlotCount(); ++slot_index) {
        auto const action_id = QStringLiteral("triage.sequence_slot_%1").arg(slot_index);
        auto const display_name = QStringLiteral("Execute Sequence Slot %1").arg(slot_index);
        auto const default_binding = QKeySequence(static_cast<Qt::Key>(Qt::Key_0 + slot_index));

        km->registerAction({.action_id = action_id,
                            .display_name = display_name,
                            .category = category,
                            .scope = scope,
                            .default_binding = default_binding,
                            .command_sequence = std::nullopt});
    }
}

void registerEditorActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }

    auto const scope = KeymapSystem::KeyActionScope::editorFocused(
            EditorLib::EditorTypeId(QStringLiteral("TriageSessionWidget")));
    QString const category = QStringLiteral("Triage Session");

    km->registerAction({.action_id = QStringLiteral("triage.mark"),
                        .display_name = QStringLiteral("Mark"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M),
                        .command_sequence = std::nullopt});

    km->registerAction({.action_id = QStringLiteral("triage.commit"),
                        .display_name = QStringLiteral("Commit"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C),
                        .command_sequence = std::nullopt});

    km->registerAction({.action_id = QStringLiteral("triage.recall"),
                        .display_name = QStringLiteral("Recall"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R),
                        .command_sequence = std::nullopt});

    registerSequenceSlotActions(km, scope, category);
}

}// namespace

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> const & data_manager,
                   commands::CommandRecorder * recorder,
                   KeymapSystem::KeymapManager * keymap_manager) {
    if (!registry) {
        return;
    }
    auto const & dm = data_manager;
    auto cr = recorder;
    auto * km = keymap_manager;

    registerEditorActions(km);

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
            .create_editor_custom = [dm, cr, km](EditorRegistry * reg)
                    -> EditorRegistry::EditorInstance {
                auto state = std::make_shared<TriageSessionState>(dm);

                auto * widget = new TriageSessionProperties_Widget(state, reg);
                widget->setCommandRecorder(cr);
                widget->setKeymapManager(km);
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
