#include "MediaWidgetRegistration.hpp"

#include "Media_Widget.hpp"
#include "MediaWidgetState.hpp"
#include "Media_Window/Media_Window.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"
#include "GroupManagementWidget/GroupManager.hpp"

#include <iostream>

namespace MediaWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager) {
    
    if (!registry) {
        std::cerr << "MediaWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto gm = group_manager;

    registry->registerType({
        .type_id = QStringLiteral("MediaWidget"),
        .display_name = QStringLiteral("Media Viewer"),
        .icon_path = QStringLiteral(":/icons/media.png"),
        .menu_path = QStringLiteral("View/Visualization"),
        .default_zone = QStringLiteral("center"),
        .allow_multiple = true,

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<MediaWidgetState>();
        },

        // View factory - for now returns nullptr since Media_Widget is the combined widget
        // Future: This will create MediaViewWidget
        .create_view = [dm, gm, registry](std::shared_ptr<EditorState> state) -> QWidget * {
            auto media_state = std::dynamic_pointer_cast<MediaWidgetState>(state);
            if (!media_state) {
                std::cerr << "MediaWidgetModule: Failed to cast state to MediaWidgetState" << std::endl;
                return nullptr;
            }

            // Currently Media_Widget is the combined view+properties widget
            // In the future, this will be split into MediaViewWidget
            auto * widget = new Media_Widget(registry);
            widget->setDataManager(dm);
            
            // Set the group manager if available
            if (gm) {
                auto * media_window = widget->getMediaWindow();
                if (media_window) {
                    media_window->setGroupManager(gm);
                }
            }

            return widget;
        },

        // Properties factory - currently nullptr since Media_Widget is combined
        // Future: This will create MediaPropertiesWidget
        .create_properties = nullptr,

        // Custom editor creation for complex view/properties coupling
        // When the view and properties widgets need to share resources
        // (like Media_Window reference), use this custom factory
        .create_editor_custom = [dm, gm, registry](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<MediaWidgetState>();

            // Create the combined widget (current architecture)
            // Future: Split into MediaViewWidget + MediaPropertiesWidget
            auto * widget = new Media_Widget(registry);
            widget->setDataManager(dm);

            // Set the group manager if available
            if (gm) {
                auto * media_window = widget->getMediaWindow();
                if (media_window) {
                    media_window->setGroupManager(gm);
                }
            }

            // Register the state
            reg->registerState(state);

            // For now, widget is both view and properties combined
            // Future: view = MediaViewWidget, properties = MediaPropertiesWidget
            return EditorRegistry::EditorInstance{
                .state = state,
                .view = widget,
                .properties = nullptr  // Will be MediaPropertiesWidget when split
            };
        }
    });

    // Note: Additional media-related types can be registered here in the future
    // For example: MediaViewerLite, MediaCompare, etc.
}

}  // namespace MediaWidgetModule
