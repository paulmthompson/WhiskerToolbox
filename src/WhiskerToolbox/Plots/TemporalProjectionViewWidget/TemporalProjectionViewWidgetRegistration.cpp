#include "TemporalProjectionViewWidgetRegistration.hpp"

#include "Core/TemporalProjectionViewState.hpp"
#include "UI/TemporalProjectionViewPropertiesWidget.hpp"
#include "UI/TemporalProjectionViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace TemporalProjectionViewWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "TemporalProjectionViewWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("TemporalProjectionViewWidget"),
                            .display_name = QStringLiteral("Temporal Projection View"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Temporal Projection View"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<TemporalProjectionViewState>(); },

                            // View factory - creates TemporalProjectionViewWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto projection_state = std::dynamic_pointer_cast<TemporalProjectionViewState>(state);
                                if (!projection_state) {
                                    std::cerr << "TemporalProjectionViewWidgetModule: Failed to cast state to TemporalProjectionViewState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new TemporalProjectionViewWidget(dm);
                                widget->setState(projection_state);

                                return widget;
                            },

                            // Properties factory - creates TemporalProjectionViewPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto projection_state = std::dynamic_pointer_cast<TemporalProjectionViewState>(state);
                                if (!projection_state) {
                                    std::cerr << "TemporalProjectionViewWidgetModule: Failed to cast state to TemporalProjectionViewState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new TemporalProjectionViewPropertiesWidget(projection_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<TemporalProjectionViewState>();

                                // Create the view widget
                                auto * view = new TemporalProjectionViewWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new TemporalProjectionViewPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the temporal projection view to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &TemporalProjectionViewWidget::timePositionSelected,
                                                     [reg](TimePosition position) {
                                                         // Update EditorRegistry time (triggers timeChanged signal for other widgets)
                                                         reg->setCurrentTime(position);
                                                     });
                                }

                                // Register the state
                                reg->registerState(state);

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = view,
                                        .properties = props};
                            }});
}

}// namespace TemporalProjectionViewWidgetModule
