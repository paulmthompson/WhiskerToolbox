#include "3DPlotWidgetRegistration.hpp"

#include "Core/3DPlotState.hpp"
#include "UI/3DPlotPropertiesWidget.hpp"
#include "UI/3DPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace ThreeDPlotWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "ThreeDPlotWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("3DPlotWidget"),
                           .display_name = QStringLiteral("3D Plot"),
                           .icon_path = QStringLiteral(""),// No icon for now
                           .menu_path = QStringLiteral("Plot/3D Plot"),
                           .preferred_zone = Zone::Center,
                           .properties_zone = Zone::Right,
                           .prefers_split = false,
                           .properties_as_tab = true,
                           .auto_raise_properties = false,
                           .allow_multiple = true,

                           // State factory - creates the shared state object
                           .create_state = []() { return std::make_shared<ThreeDPlotState>(); },

                           // View factory - creates ThreeDPlotWidget (the view component)
                           .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                               auto plot_state = std::dynamic_pointer_cast<ThreeDPlotState>(state);
                               if (!plot_state) {
                                   std::cerr << "ThreeDPlotWidgetModule: Failed to cast state to ThreeDPlotState" << std::endl;
                                   return nullptr;
                               }

                               auto * widget = new ThreeDPlotWidget(dm);
                               widget->setState(plot_state);

                               // Connect to global time changes for plot updates
                               if (reg) {
                                   QObject::connect(reg,
                                                    QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                    widget, &ThreeDPlotWidget::_onTimeChanged);
                               }

                               return widget;
                           },

                           // Properties factory - creates ThreeDPlotPropertiesWidget
                           .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                               auto plot_state = std::dynamic_pointer_cast<ThreeDPlotState>(state);
                               if (!plot_state) {
                                   std::cerr << "ThreeDPlotWidgetModule: Failed to cast state to ThreeDPlotState for properties" << std::endl;
                                   return nullptr;
                               }

                               // Create properties widget with shared state
                               auto * props = new ThreeDPlotPropertiesWidget(plot_state, dm);
                               return props;
                           },

                           // Custom editor creation for view/properties coupling
                           .create_editor_custom = [dm](EditorRegistry * reg)
                                   -> EditorRegistry::EditorInstance {
                               // Create the shared state
                               auto state = std::make_shared<ThreeDPlotState>();

                               // Create the view widget
                               auto * view = new ThreeDPlotWidget(dm);
                               view->setState(state);

                               // Connect to global time changes for plot updates
                               if (reg) {
                                   QObject::connect(reg,
                                                    QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                    view, &ThreeDPlotWidget::_onTimeChanged);
                               }

                               // Create the properties widget with the shared state
                               auto * props = new ThreeDPlotPropertiesWidget(state, dm);

                               // Connect properties widget to view widget
                               props->setPlotWidget(view);

                               // Connect view widget time position selection to update time in EditorRegistry
                               // This allows the 3D plot to navigate to a specific time position
                               if (reg) {
                                   QObject::connect(view, &ThreeDPlotWidget::timePositionSelected,
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

}// namespace ThreeDPlotWidgetModule
