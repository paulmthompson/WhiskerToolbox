#include "OnionSkinViewWidgetRegistration.hpp"

#include "Core/OnionSkinViewState.hpp"
#include "UI/OnionSkinViewPropertiesWidget.hpp"
#include "UI/OnionSkinViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace OnionSkinViewWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "OnionSkinViewWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("OnionSkinViewWidget"),
                            .display_name = QStringLiteral("Onion Skin View"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Onion Skin View"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<OnionSkinViewState>(); },

                            // View factory - creates TemporalProjectionViewWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto onion_skin_state = std::dynamic_pointer_cast<OnionSkinViewState>(state);
                                if (!onion_skin_state) {
                                    std::cerr << "OnionSkinViewWidgetModule: Failed to cast state to OnionSkinViewState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new OnionSkinViewWidget(dm);
                                widget->setState(onion_skin_state);

                                if (reg) {
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     widget, &OnionSkinViewWidget::_onTimeChanged);
                                }

                                return widget;
                            },

                            // Properties factory - creates TemporalProjectionViewPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto onion_skin_state = std::dynamic_pointer_cast<OnionSkinViewState>(state);
                                if (!onion_skin_state) {
                                    std::cerr << "OnionSkinViewWidgetModule: Failed to cast state to OnionSkinViewState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new OnionSkinViewPropertiesWidget(onion_skin_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<OnionSkinViewState>();

                                // Create the view widget
                                auto * view = new OnionSkinViewWidget(dm);
                                view->setState(state);

                                if (reg) {
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     view, &OnionSkinViewWidget::_onTimeChanged);
                                }

                                // Create the properties widget with the shared state
                                auto * props = new OnionSkinViewPropertiesWidget(state, dm);
                                props->setPlotWidget(view);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the temporal projection view to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &OnionSkinViewWidget::timePositionSelected,
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

}// namespace OnionSkinViewWidgetModule
