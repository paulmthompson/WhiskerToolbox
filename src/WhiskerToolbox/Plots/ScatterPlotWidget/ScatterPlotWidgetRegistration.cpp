#include "ScatterPlotWidgetRegistration.hpp"

#include "Core/ScatterPlotState.hpp"
#include "UI/ScatterPlotPropertiesWidget.hpp"
#include "UI/ScatterPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace ScatterPlotWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "ScatterPlotWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("ScatterPlotWidget"),
                            .display_name = QStringLiteral("Scatter Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Scatter Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<ScatterPlotState>(); },

                            // View factory - creates ScatterPlotWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "ScatterPlotWidgetModule: Failed to cast state to ScatterPlotState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new ScatterPlotWidget(dm);
                                widget->setState(plot_state);

                                return widget;
                            },

                            // Properties factory - creates ScatterPlotPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "ScatterPlotWidgetModule: Failed to cast state to ScatterPlotState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new ScatterPlotPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<ScatterPlotState>();

                                // Create the view widget
                                auto * view = new ScatterPlotWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new ScatterPlotPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the scatter plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &ScatterPlotWidget::timePositionSelected,
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

}// namespace ScatterPlotWidgetModule
