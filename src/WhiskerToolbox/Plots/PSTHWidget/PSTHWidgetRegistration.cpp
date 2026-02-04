#include "PSTHWidgetRegistration.hpp"

#include "Core/PSTHState.hpp"
#include "UI/PSTHPropertiesWidget.hpp"
#include "UI/PSTHWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace PSTHWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "PSTHWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("PSTHWidget"),
                            .display_name = QStringLiteral("PSTH Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/PSTH Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<PSTHState>(); },

                            // View factory - creates PSTHWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<PSTHState>(state);
                                if (!plot_state) {
                                    std::cerr << "PSTHWidgetModule: Failed to cast state to PSTHState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new PSTHWidget(dm);
                                widget->setState(plot_state);

                                return widget;
                            },

                            // Properties factory - creates PSTHPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<PSTHState>(state);
                                if (!plot_state) {
                                    std::cerr << "PSTHWidgetModule: Failed to cast state to PSTHState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new PSTHPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<PSTHState>();

                                // Create the view widget
                                auto * view = new PSTHWidget(dm);
                                view->setState(state);

                                // Create the properties widget with the shared state
                                auto * props = new PSTHPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the PSTH plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &PSTHWidget::timePositionSelected,
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

}// namespace PSTHWidgetModule
