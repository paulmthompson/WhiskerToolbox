#include "TimeScrollBarRegistration.hpp"

#include "TimeScrollBar.hpp"
#include "TimeScrollBarState.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <QKeySequence>

#include <iostream>

namespace TimeScrollBarModule {

namespace {

static void registerEditorActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }

    auto const scope = KeymapSystem::KeyActionScope::alwaysRouted(
            EditorLib::EditorTypeId(QStringLiteral("TimeScrollBar")));
    QString const category = QStringLiteral("Timeline");

    km->registerAction({.action_id = QStringLiteral("timeline.play_pause"),
                        .display_name = QStringLiteral("Play / Pause"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Space)});

    km->registerAction({.action_id = QStringLiteral("timeline.next_frame"),
                        .display_name = QStringLiteral("Next Frame"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Right)});

    km->registerAction({.action_id = QStringLiteral("timeline.prev_frame"),
                        .display_name = QStringLiteral("Previous Frame"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::Key_Left)});

    km->registerAction({.action_id = QStringLiteral("timeline.jump_forward"),
                        .display_name = QStringLiteral("Jump Forward"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::Key_Right)});

    km->registerAction({.action_id = QStringLiteral("timeline.jump_backward"),
                        .display_name = QStringLiteral("Jump Backward"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence(Qt::CTRL | Qt::Key_Left)});
}

}  // namespace

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   KeymapSystem::KeymapManager * keymap_manager) {
    
    if (!registry) {
        std::cerr << "TimeScrollBarModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Register configurable keyboard shortcuts for timeline navigation
    registerEditorActions(keymap_manager);

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TimeScrollBar"),
        .display_name = QStringLiteral("Timeline"),
        .icon_path = QStringLiteral(":/icons/timeline.png"),
        .menu_path = QStringLiteral("View/Timeline"),
        
        // Zone placement: TimeScrollBar goes to bottom zone
        // It is the global timeline control widget
        .preferred_zone = Zone::Bottom,   // Main widget goes to bottom zone
        .properties_zone = Zone::Bottom,  // No separate properties
        .prefers_split = false,
        .properties_as_tab = false,       // Not a tab - fills the zone
        .auto_raise_properties = false,   // No properties to raise
        
        .allow_multiple = false,          // Single instance only (global timeline)

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<TimeScrollBarState>();
        },

        // View factory - nullptr since we use custom editor creation
        .create_view = nullptr,

        // Properties factory - nullptr since this widget has no separate properties
        .create_properties = nullptr,

        // Custom editor creation for DataManager access
        .create_editor_custom = [dm](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<TimeScrollBarState>();

            // Create the widget with DataManager and state
            auto * widget = new TimeScrollBar(dm, state, nullptr);

            // Set EditorRegistry for time synchronization
            widget->setEditorRegistry(reg);

            // Initialize TimeFrame from DataManager (default to "time" TimeKey)
            if (dm) {
                TimeKey default_key("time");
                auto tf = dm->getTime(default_key);
                if (tf) {
                    widget->setTimeFrame(tf, default_key);
                } else {
                    // Fallback: try to get any available TimeFrame
                    auto keys = dm->getTimeFrameKeys();
                    if (!keys.empty()) {
                        auto fallback_tf = dm->getTime(keys[0]);
                        if (fallback_tf) {
                            widget->setTimeFrame(fallback_tf, keys[0]);
                        }
                    }
                }
            }

            // Register the state
            reg->registerState(state);

            // TimeScrollBar is a single widget (no view/properties split)
            // It goes into the "view" slot since that's what gets placed in preferred_zone
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr
            };
        }
    });
}

}  // namespace TimeScrollBarModule
