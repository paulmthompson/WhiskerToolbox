/**
 * @file TriageSessionWidgetRegistration.cpp
 * @brief Registration of the TriageSession widget with the EditorRegistry
 */

#include "TriageSessionWidgetRegistration.hpp"

#include "Commands/Core/CommandDescriptor.hpp"
#include "Core/TriageSessionState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "UI/TriageSessionProperties_Widget.hpp"

#include <QKeySequence>

#include <rfl/json.hpp>

#include <cassert>
#include <utility>

namespace TriageSessionWidgetModule {

namespace {

commands::CommandDescriptor makeCommand(char const * command_name, char const * params_json) {
    commands::CommandDescriptor step;
    step.command_name = command_name;
    auto params = rfl::json::read<rfl::Generic>(params_json);
    assert(params.has_value() && "makeCommand: invalid JSON parameters");
    step.parameters = std::move(*params);
    return step;
}

commands::CommandSequenceDescriptor sequenceContactMarkTrackedAdvance() {
    commands::CommandSequenceDescriptor seq;
    seq.name = "contact_mark_tracked_advance";
    seq.commands = {
            makeCommand("SetEventAtTime",
                        R"({"data_key": "contact", "time": "${current_frame}", "event": true})"),
            makeCommand("SetEventAtTime",
                        R"({"data_key": "tracked_regions", "time": "${current_frame}", "event": true})"),
            makeCommand("AdvanceFrame", R"({"delta": 1})"),
    };
    return seq;
}

commands::CommandSequenceDescriptor sequenceContactFlipTrackedAdvance() {
    commands::CommandSequenceDescriptor seq;
    seq.name = "contact_flip_tracked_advance";
    seq.commands = {
            makeCommand("FlipEventAtTime",
                        R"({"data_key": "contact", "time": "${current_frame}"})"),
            makeCommand("SetEventAtTime",
                        R"({"data_key": "tracked_regions", "time": "${current_frame}", "event": true})"),
            makeCommand("AdvanceFrame", R"({"delta": 1})"),
    };
    return seq;
}

commands::CommandSequenceDescriptor sequenceTrackedAdvance() {
    commands::CommandSequenceDescriptor seq;
    seq.name = "tracked_advance";
    seq.commands = {
            makeCommand("SetEventAtTime",
                        R"({"data_key": "tracked_regions", "time": "${current_frame}", "event": true})"),
            makeCommand("AdvanceFrame", R"({"delta": 1})"),
    };
    return seq;
}

/// Phase 2 contact triage chords (global scope — available outside the triage panel).
void registerContactTriageChordActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }

    auto const scope = KeymapSystem::KeyActionScope::global();
    QString const category = QStringLiteral("Contact Triage");

    km->registerCommandAction(QStringLiteral("triage.contact_mark_tracked_advance"),
                              QStringLiteral("Mark contact + tracked + next frame"),
                              category,
                              scope,
                              QKeySequence(Qt::Key_F),
                              sequenceContactMarkTrackedAdvance());

    km->registerCommandAction(QStringLiteral("triage.contact_flip_tracked_advance"),
                              QStringLiteral("Flip contact + tracked + next frame"),
                              category,
                              scope,
                              QKeySequence(Qt::Key_D),
                              sequenceContactFlipTrackedAdvance());

    km->registerCommandAction(QStringLiteral("triage.tracked_advance"),
                              QStringLiteral("Mark tracked + next frame"),
                              category,
                              scope,
                              QKeySequence(Qt::Key_G),
                              sequenceTrackedAdvance());
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
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M)});

    km->registerAction({.action_id = QStringLiteral("triage.commit"),
                        .display_name = QStringLiteral("Commit"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)});

    km->registerAction({.action_id = QStringLiteral("triage.recall"),
                        .display_name = QStringLiteral("Recall"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R)});

    registerContactTriageChordActions(km);
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
