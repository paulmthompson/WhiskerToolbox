#include "MLWidgetRegistration.hpp"

#include "ML_Widget.hpp"
#include "MLWidgetState.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "DataManager/DataManager.hpp"

#include <iostream>

namespace MLWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager) {
    
    if (!registry) {
        std::cerr << "MLWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto dm = data_manager;

    registry->registerType({
        .type_id = QStringLiteral("MLWidget"),
        .display_name = QStringLiteral("Machine Learning"),
        .icon_path = QString{},
        .menu_path = QStringLiteral("View/Analysis"),
        
        // Zone placement: ML_Widget is a tool widget in the right zone
        // It has no separate "view" - the widget itself is the tool panel
        .preferred_zone = Zone::Right,    // Main widget goes to right zone
        .properties_zone = Zone::Right,   // No separate properties
        .prefers_split = false,
        .properties_as_tab = true,        // Add as tab in the zone
        .auto_raise_properties = false,   // Don't auto-raise
        
        .allow_multiple = false,          // Single instance only

        // State factory - creates the shared state object
        .create_state = []() {
            return std::make_shared<MLWidgetState>();
        },

        // View factory - creates the ML_Widget
        // Note: ML_Widget is a self-contained tool widget
        .create_view = [dm](std::shared_ptr<EditorState> state) -> QWidget * {
            auto ml_state = std::dynamic_pointer_cast<MLWidgetState>(state);
            if (!ml_state) {
                std::cerr << "MLWidgetModule: Failed to cast state to MLWidgetState" << std::endl;
                return nullptr;
            }
            
            // Create ML_Widget with DataManager
            auto * widget = new ML_Widget(dm);
            
            // Set size constraints suitable for right zone
            widget->setMinimumSize(400, 600);
            widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
            
            // Call openWidget to initialize the widget
            widget->openWidget();
            
            return widget;
        },

        // Properties factory - nullptr since this widget has no separate properties
        // The ML_Widget itself is the tool panel
        .create_properties = nullptr,

        // No custom editor creation needed
        .create_editor_custom = nullptr
    });
}

}  // namespace MLWidgetModule
