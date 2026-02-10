#include "ACFWidgetRegistration.hpp"

#include "Core/ACFState.hpp"
#include "UI/ACFPropertiesWidget.hpp"
#include "UI/ACFWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace ACFWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "ACFWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("ACFWidget"),
                            .display_name = QStringLiteral("Autocorrelation Function"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Autocorrelation Function"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<ACFState>(); },

                            // View factory - creates ACFWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto acf_state = std::dynamic_pointer_cast<ACFState>(state);
                                if (!acf_state) {
                                    std::cerr << "ACFWidgetModule: Failed to cast state to ACFState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new ACFWidget(dm);
                                widget->setState(acf_state);

                                return widget;
                            },

                            // Properties factory - creates ACFPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto acf_state = std::dynamic_pointer_cast<ACFState>(state);
                                if (!acf_state) {
                                    std::cerr << "ACFWidgetModule: Failed to cast state to ACFState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new ACFPropertiesWidget(acf_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<ACFState>();

                                // Create the view widget
                                auto * view = new ACFWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new ACFPropertiesWidget(state, dm);
                                props->setPlotWidget(view);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the ACF widget to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &ACFWidget::timePositionSelected,
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

}// namespace ACFWidgetModule
