#include "ExportVideoWidgetRegistration.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "ExportVideoWidgetState.hpp"
#include "Export_Video_Widget.hpp"
#include "TimeFrame/TimeFrame.hpp"// For TimePosition

#include <iostream>

namespace ExportVideoWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "ExportVideoWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({.type_id = QStringLiteral("ExportVideoWidget"),
                            .display_name = QStringLiteral("Video Export"),
                            .icon_path = QStringLiteral(":/icons/video_export.png"),
                            .menu_path = QStringLiteral("Export/Video"),

                            // Zone placement: ExportVideoWidget is a tool widget in the right zone
                            // It has no separate "view" - the widget itself is the tool
                            .preferred_zone = Zone::Right, // Main widget goes to right zone
                            .properties_zone = Zone::Right,// No separate properties
                            .prefers_split = false,
                            .properties_as_tab = true,    // Add as tab in the zone
                            .auto_raise_properties = true,// Auto-raise when opened

                            .allow_multiple = false,// Single instance only

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<ExportVideoWidgetState>(); },

                            // View factory - nullptr since we use custom editor creation
                            .create_view = nullptr,

                            // Properties factory - nullptr since this widget has no separate properties
                            .create_properties = nullptr,

                            // Custom editor creation for EditorRegistry access
                            // Export_Video_Widget needs EditorRegistry to access MediaWidget states
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<ExportVideoWidgetState>();

                                // Create the widget with EditorRegistry for MediaWidget state access
                                auto * widget = new Export_Video_Widget(dm, reg, state, nullptr);

                                // Connect widget's requestTimeChange signal to EditorRegistry
                                // This allows the widget to request time changes during video export
                                QObject::connect(widget, &Export_Video_Widget::requestTimeChange,
                                                 [reg](TimePosition position) {
                                                     if (reg) {
                                                         reg->setCurrentTime(position);
                                                     }
                                                 });

                                // Set explicit minimum size constraints
                                widget->setMinimumSize(400, 600);
                                widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

                                // Register the state
                                reg->registerState(state);

                                // Export_Video_Widget is a single widget (no view/properties split)
                                // It goes into the "view" slot since that's what gets placed in preferred_zone
                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = widget,
                                        .properties = nullptr};
                            }});
}

}// namespace ExportVideoWidgetModule
