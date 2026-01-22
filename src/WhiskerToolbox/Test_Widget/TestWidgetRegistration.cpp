#include "TestWidgetRegistration.hpp"

#include "TestWidgetState.hpp"
#include "TestWidgetView.hpp"
#include "TestWidgetProperties.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace TestWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "TestWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("TestWidget"),
        .display_name = QStringLiteral("Test Widget"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Development"),
        
        // Zone placement: TestWidget demonstrates View/Properties split
        // View (TestWidgetView) goes to center zone
        // Properties (TestWidgetProperties) goes to right zone as a tab
        .preferred_zone = Zone::Center,
        .properties_zone = Zone::Right,
        .prefers_split = false,
        .properties_as_tab = true,
        .auto_raise_properties = true,   // Show properties when test widget opens
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object with DataManager
        .create_state = [dm]() {
            return std::make_shared<TestWidgetState>(dm);
        },

        // View factory - creates the TestWidgetView visualization component
        .create_view = [](std::shared_ptr<EditorState> state) -> QWidget * {
            auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
            if (!test_state) {
                std::cerr << "TestWidgetModule: Failed to cast state to TestWidgetState" << std::endl;
                return nullptr;
            }
            return new TestWidgetView(test_state);
        },

        // Properties factory - creates the TestWidgetProperties controls component
        .create_properties = [](std::shared_ptr<EditorState> state) -> QWidget * {
            auto test_state = std::dynamic_pointer_cast<TestWidgetState>(state);
            if (!test_state) {
                std::cerr << "TestWidgetModule: Failed to cast state to TestWidgetState (properties)" << std::endl;
                return nullptr;
            }
            return new TestWidgetProperties(test_state);
        },

        // No custom editor creation needed - standard factories handle everything
        .create_editor_custom = nullptr
    });
}

}  // namespace TestWidgetModule
