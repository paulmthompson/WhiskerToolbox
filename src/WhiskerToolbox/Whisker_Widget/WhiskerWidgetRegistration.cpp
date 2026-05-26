#include "WhiskerWidgetRegistration.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/StrongTypes.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "WhiskerWidgetState.hpp"
#include "Whisker_Widget.hpp"

#include <QKeySequence>

#include <iostream>
#include <utility>

namespace WhiskerWidgetModule {

namespace {

void registerEditorActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }

    // AlwaysRouted so Trace fires while Whisker Tracking is open, even when another editor is active
    auto const scope = KeymapSystem::KeyActionScope::alwaysRouted(
            EditorLib::EditorTypeId(QStringLiteral("WhiskerWidget")));
    QString const category = QStringLiteral("Whisker Tracking");

    km->registerAction({.action_id = QStringLiteral("whisker_widget.trace"),
                        .display_name = QStringLiteral("Trace"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_T)});
}

}// namespace

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   KeymapSystem::KeymapManager * keymap_manager) {

    if (!registry) {
        std::cerr << "WhiskerWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    registerEditorActions(keymap_manager);

    // Capture dependencies for lambdas
    const auto& dm = std::move(data_manager);
    auto * km = keymap_manager;

    registry->registerType({.type_id = QStringLiteral("WhiskerWidget"),
                            .display_name = QStringLiteral("Whisker Tracking"),
                            .icon_path = QStringLiteral(":/icons/whisker.png"),
                            .menu_path = QStringLiteral("Analysis/Whisker"),

                            // Zone placement: WhiskerWidget is a tool widget in the right zone
                            // It has no separate "view" - the widget itself is the tool
                            .preferred_zone = Zone::Right, // Main widget goes to right zone
                            .properties_zone = Zone::Right,// No separate properties
                            .prefers_split = false,
                            .properties_as_tab = true,    // Add as tab in the zone
                            .auto_raise_properties = true,// Auto-raise when opened

                            .allow_multiple = false,// Single instance only

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<WhiskerWidgetState>(); },

                            // View factory - nullptr since we use custom editor creation
                            .create_view = nullptr,

                            // Properties factory - nullptr since this widget has no separate properties
                            .create_properties = nullptr,

                            // Custom editor creation for DataManager and TimeScrollBar access
                            .create_editor_custom = [dm, km](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<WhiskerWidgetState>();

                                // Create the widget with DataManager
                                auto * widget = new Whisker_Widget(dm, state, nullptr);

                                // Set explicit minimum size constraints for proper docking
                                widget->setMinimumSize(400, 500);
                                widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

                                if (km) {
                                    widget->setKeymapManager(km);
                                }

                                // Register the state
                                reg->registerState(state);

                                // Initialize current_position from EditorRegistry's current position
                                if (reg) {
                                    state->current_position = reg->currentPosition();

                                    // Connect EditorRegistry time changes to update state's current_position
                                    // This allows sub-widgets to access the current time position from the state
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     [state](TimePosition position) {
                                                         state->current_position = std::move(position);
                                                     });
                                }

                                // Initialize the widget (calls openWidget internally handled setup)
                                widget->openWidget();

                                // Whisker_Widget is a single widget (no view/properties split)
                                // It goes into the "view" slot since that's what gets placed in preferred_zone
                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = widget,
                                        .properties = nullptr};
                            }});
}

}// namespace WhiskerWidgetModule
