#include "SpectrogramWidgetRegistration.hpp"

#include "Core/SpectrogramState.hpp"
#include "UI/SpectrogramPropertiesWidget.hpp"
#include "UI/SpectrogramWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace SpectrogramWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "SpectrogramWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("SpectrogramWidget"),
                            .display_name = QStringLiteral("Spectrogram"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Spectrogram"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<SpectrogramState>(); },

                            // View factory - creates SpectrogramWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<SpectrogramState>(state);
                                if (!plot_state) {
                                    std::cerr << "SpectrogramWidgetModule: Failed to cast state to SpectrogramState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new SpectrogramWidget(dm);
                                widget->setState(plot_state);

                                if (reg) {
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     widget, &SpectrogramWidget::_onTimeChanged);
                                }

                                return widget;
                            },

                            // Properties factory - creates SpectrogramPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<SpectrogramState>(state);
                                if (!plot_state) {
                                    std::cerr << "SpectrogramWidgetModule: Failed to cast state to SpectrogramState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new SpectrogramPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<SpectrogramState>();

                                // Create the view widget
                                auto * view = new SpectrogramWidget(dm);
                                view->setState(state);

                                if (reg) {
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     view, &SpectrogramWidget::_onTimeChanged);
                                }

                                // Create the properties widget with the shared state
                                auto * props = new SpectrogramPropertiesWidget(state, dm);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the spectrogram to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &SpectrogramWidget::timePositionSelected,
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

}// namespace SpectrogramWidgetModule
