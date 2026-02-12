#include "DeepLearningWidgetRegistration.hpp"

#include "DeepLearning_Widget/Core/DeepLearningState.hpp"
#include "DeepLearning_Widget/UI/DeepLearningPropertiesWidget.hpp"
#include "DeepLearning_Widget/UI/DeepLearningViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>

namespace DeepLearningWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    if (!registry) {
        std::cerr << "DeepLearningWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    auto dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("DeepLearningWidget"),
                            .display_name = QStringLiteral("Deep Learning"),
                            .icon_path = QString{},// No icon for now
                            .menu_path = QStringLiteral("View/Analysis"),

                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,

                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<DeepLearningState>(); },

                            // View factory - creates DeepLearningViewWidget (main visualization)
                            .create_view = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto dl_state = std::dynamic_pointer_cast<DeepLearningState>(state);
                                if (!dl_state) {
                                    std::cerr << "DeepLearningWidgetModule: Failed to cast state to DeepLearningState" << std::endl;
                                    return nullptr;
                                }

                                auto * view = new DeepLearningViewWidget(dl_state, dm);
                                return view;
                            },

                            // Properties factory - creates DeepLearningPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
                                auto dl_state = std::dynamic_pointer_cast<DeepLearningState>(state);
                                if (!dl_state) {
                                    std::cerr << "DeepLearningWidgetModule: Failed to cast state to DeepLearningState (properties)" << std::endl;
                                    return nullptr;
                                }

                                auto * props = new DeepLearningPropertiesWidget(dl_state, dm);
                                return props;
                            },

                            // Custom editor creation to connect time signal
                            .create_editor_custom = [dm, reg](EditorRegistry * /*unused*/)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<DeepLearningState>();

                                // Create the view widget
                                auto * view = new DeepLearningViewWidget(state, dm);

                                // Create the properties widget with the shared state
                                auto * props = new DeepLearningPropertiesWidget(state, dm);

                                // Connect EditorRegistry time changes to properties widget
                                if (reg) {
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     props, &DeepLearningPropertiesWidget::onTimeChanged);
                                }

                                // Register the state
                                reg->registerState(state);

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = view,
                                        .properties = props};
                            }});
}

}// namespace DeepLearningWidgetModule
