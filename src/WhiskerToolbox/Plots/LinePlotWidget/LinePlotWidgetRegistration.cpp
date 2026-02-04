#include "LinePlotWidgetRegistration.hpp"

#include "Core/LinePlotState.hpp"
#include "UI/LinePlotPropertiesWidget.hpp"
#include "UI/LinePlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace LinePlotWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "LinePlotWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("LinePlotWidget"),
                            .display_name = QStringLiteral("Line Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Line Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<LinePlotState>(); },

                            // View factory - creates LinePlotWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<LinePlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "LinePlotWidgetModule: Failed to cast state to LinePlotState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new LinePlotWidget(dm);
                                widget->setState(plot_state);

                                return widget;
                            },

                            // Properties factory - creates LinePlotPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<LinePlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "LinePlotWidgetModule: Failed to cast state to LinePlotState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new LinePlotPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<LinePlotState>();

                                // Create the view widget
                                auto * view = new LinePlotWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new LinePlotPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the line plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &LinePlotWidget::timePositionSelected,
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

}// namespace LinePlotWidgetModule
