#include "PythonWidgetRegistration.hpp"

#include "Core/PythonWidgetState.hpp"
#include "UI/PythonViewWidget.hpp"
#include "UI/PythonPropertiesWidget.hpp"

#include "EditorState/EditorRegistry.hpp"

#include <iostream>

namespace PythonWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {

    if (!registry) {
        std::cerr << "PythonWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("PythonWidget"),
        .display_name = QStringLiteral("Python Console"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Tools"),
        .preferred_zone = Zone::Center,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = false,
        .allow_multiple = false,

        // State factory
        .create_state = []() {
            return std::make_shared<PythonWidgetState>();
        },

        // View factory
        .create_view = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto python_state = std::dynamic_pointer_cast<PythonWidgetState>(state);
            if (!python_state) {
                std::cerr << "PythonWidgetModule: Failed to cast state to PythonWidgetState" << std::endl;
                return nullptr;
            }
            return new PythonViewWidget(python_state);
        },

        // Properties factory
        .create_properties = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto python_state = std::dynamic_pointer_cast<PythonWidgetState>(state);
            if (!python_state) {
                std::cerr << "PythonWidgetModule: Failed to cast state to PythonWidgetState for properties" << std::endl;
                return nullptr;
            }
            return new PythonPropertiesWidget(python_state);
        },

        .create_editor_custom = nullptr
    });
}

}  // namespace PythonWidgetModule
