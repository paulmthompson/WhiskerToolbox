#include "HeatmapWidgetRegistration.hpp"

#include "Core/HeatmapState.hpp"
#include "UI/HeatmapPropertiesWidget.hpp"
#include "UI/HeatmapWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace HeatmapWidgetModule {

void registerTypes(EditorRegistry * registry,
                  std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "HeatmapWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("HeatmapWidget"),
                            .display_name = QStringLiteral("Heatmap Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Heatmap Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<HeatmapState>(); },

                            // View factory - creates HeatmapWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<HeatmapState>(state);
                                if (!plot_state) {
                                    std::cerr << "HeatmapWidgetModule: Failed to cast state to HeatmapState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new HeatmapWidget(dm);
                                widget->setState(plot_state);

                                return widget;
                            },

                            // Properties factory - creates HeatmapPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<HeatmapState>(state);
                                if (!plot_state) {
                                    std::cerr << "HeatmapWidgetModule: Failed to cast state to HeatmapState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new HeatmapPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<HeatmapState>();

                                // Create the view widget
                                auto * view = new HeatmapWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new HeatmapPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the heatmap plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &HeatmapWidget::timePositionSelected,
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

}// namespace HeatmapWidgetModule
