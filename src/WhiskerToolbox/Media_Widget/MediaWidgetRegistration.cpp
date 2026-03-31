#include "MediaWidgetRegistration.hpp"

#include "Core/MediaWidgetState.hpp"
#include "Rendering/Media_Window/Media_Window.hpp"
#include "UI/MediaPropertiesWidget.hpp"
#include "UI/Media_Widget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <QKeySequence>

#include <iostream>
#include <utility>

namespace MediaWidgetModule {

/// @brief Register the 9 group-assignment keyboard actions with the keymap manager
static void registerGroupActions(KeymapSystem::KeymapManager * keymap_manager) {
    if (!keymap_manager) {
        return;
    }

    auto const scope = KeymapSystem::KeyActionScope::editorFocused(
            EditorLib::EditorTypeId(QStringLiteral("MediaWidget")));
    QString const category = QStringLiteral("Media Viewer");

    // Map keys 1–9 to group assignment actions
    constexpr std::array<Qt::Key, 9> keys = {
            Qt::Key_1, Qt::Key_2, Qt::Key_3,
            Qt::Key_4, Qt::Key_5, Qt::Key_6,
            Qt::Key_7, Qt::Key_8, Qt::Key_9};

    for (int i = 0; i < 9; ++i) {
        keymap_manager->registerAction({.action_id = QStringLiteral("media.assign_group_%1").arg(i + 1),
                                        .display_name = QStringLiteral("Assign to Group %1").arg(i + 1),
                                        .category = category,
                                        .scope = scope,
                                        .default_binding = QKeySequence(keys[static_cast<size_t>(i)])});
    }
}

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager,
                   KeymapSystem::KeymapManager * keymap_manager) {

    if (!registry) {
        std::cerr << "MediaWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Register configurable keyboard shortcuts for group assignment
    registerGroupActions(keymap_manager);

    // Capture dependencies for lambdas
    const auto& dm = std::move(data_manager);
    auto gm = group_manager;
    auto * km = keymap_manager;

    registry->registerType({.type_id = QStringLiteral("MediaWidget"),
                            .display_name = QStringLiteral("Media Viewer"),
                            .icon_path = QStringLiteral(":/icons/media.png"),
                            .menu_path = QStringLiteral("View/Visualization"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<MediaWidgetState>(); },

                            // View factory - creates Media_Widget (the canvas/view component)
                            .create_view = [dm, gm, km, registry](const std::shared_ptr<EditorState>& state) -> QWidget * {
                                auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(state);
                                if (!media_state) {
                                    std::cerr << "MediaWidgetModule: Failed to cast state to MediaWidgetState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new Media_Widget(registry);
                                widget->setDataManager(dm);

                                // Set the group manager if available
                                if (gm) {
                                    auto * media_window = widget->getMediaWindow();
                                    if (media_window) {
                                        media_window->setGroupManager(gm);
                                        media_window->setKeymapManager(km);
                                    }
                                }

                                return widget;
                            },

                            // Properties factory - creates MediaPropertiesWidget
                            .create_properties = [dm](const std::shared_ptr<EditorState>& state) -> QWidget * {
                                auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(state);
                                if (!media_state) {
                                    std::cerr << "MediaWidgetModule: Failed to cast state to MediaWidgetState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                // Media_Window will be set later via setMediaWindow() after view is created
                                auto * props = new MediaPropertiesWidget(media_state, dm, nullptr);
                                return props;
                            },

                            // Custom editor creation for complex view/properties coupling
                            // The custom factory ensures properties gets a reference to Media_Window
                            .create_editor_custom = [dm, gm, km, registry](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the view widget first - it creates its own internal state
                                auto * view = new Media_Widget(registry);
                                view->setDataManager(dm);

                                // Get the state from the view widget - this ensures all components
                                // share the same state instance that receives LoadFrame updates
                                auto state = view->getState();

                                // Set the group manager if available
                                if (gm) {
                                    auto * media_window = view->getMediaWindow();
                                    if (media_window) {
                                        media_window->setGroupManager(gm);
                                        media_window->setKeymapManager(km);
                                    }
                                }

                                // Create the properties widget with the shared state from the view
                                // Pass Media_Window from the view for coordination
                                auto * props = new MediaPropertiesWidget(state, dm, view->getMediaWindow());

                                // Connect properties widget signals to view
                                QObject::connect(props, &MediaPropertiesWidget::featureEnabledChanged,
                                                 view, &Media_Widget::setFeatureEnabled);

                                // Note: State is already registered by Media_Widget constructor,
                                // no need to register it again

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = view,
                                        .properties = props};
                            }});

    // Note: Additional media-related types can be registered here in the future
    // For example: MediaViewerLite, MediaCompare, etc.
}

}// namespace MediaWidgetModule
