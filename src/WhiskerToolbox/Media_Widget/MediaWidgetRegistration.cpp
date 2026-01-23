#include "MediaWidgetRegistration.hpp"

#include "Media_Widget.hpp"
#include "MediaWidgetState.hpp"
#include "MediaPropertiesWidget/MediaPropertiesWidget.hpp"
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
        .preferred_zone = Zone::Center,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = false,
        .allow_multiple = true,

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<MediaWidgetState>();
        },

        // View factory - creates Media_Widget (the canvas/view component)
        .create_view = [dm, gm, registry](std::shared_ptr<EditorState> state) -> QWidget * {
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
                }
            }

            return widget;
        },

        // Properties factory - creates MediaPropertiesWidget
        .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
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
        .create_editor_custom = [dm, gm, registry](EditorRegistry * reg) 
            -> EditorRegistry::EditorInstance 
        {
            // Create the shared state
            auto state = std::make_shared<MediaWidgetState>();

            // Create the view widget
            auto * view = new Media_Widget(registry);
            view->setDataManager(dm);

            // Set the group manager if available
            if (gm) {
                auto * media_window = view->getMediaWindow();
                if (media_window) {
                    media_window->setGroupManager(gm);
                }
            }

            // Create the properties widget with the shared state
            // Pass Media_Window from the view for coordination
            auto * props = new MediaPropertiesWidget(state, dm, view->getMediaWindow());

            // Connect properties widget signals to view
            QObject::connect(props, &MediaPropertiesWidget::featureEnabledChanged,
                             view, &Media_Widget::setFeatureEnabled);

            // Register the state
            reg->registerState(state);

            return EditorRegistry::EditorInstance{
                .state = state,
                .view = view,
                .properties = props
            };
        }
    });

    // Note: Additional media-related types can be registered here in the future
    // For example: MediaViewerLite, MediaCompare, etc.
}

}  // namespace MediaWidgetModule
