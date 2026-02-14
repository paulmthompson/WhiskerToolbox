#include "PythonWidgetRegistration.hpp"

#include "Core/PythonWidgetState.hpp"
#include "UI/PythonViewWidget.hpp"
#include "UI/PythonPropertiesWidget.hpp"
#include "UI/PythonConsoleWidget.hpp"

#include "EditorState/EditorRegistry.hpp"

#include <QPlainTextEdit>
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

        // View factory — nullptr since we use create_editor_custom
        .create_view = nullptr,

        // Properties factory — nullptr since we use create_editor_custom
        .create_properties = nullptr,

        // Custom editor creation: view and properties need to share the PythonBridge
        .create_editor_custom = [dm](EditorRegistry * reg)
            -> EditorRegistry::EditorInstance
        {
            // Create the shared state
            auto state = std::make_shared<PythonWidgetState>();

            // Create the view widget (owns PythonEngine + PythonBridge)
            auto * view = new PythonViewWidget(state, dm);

            // Create the properties widget, sharing the bridge from the view
            auto * props = new PythonPropertiesWidget(
                state, view->bridge(), dm);

            // Connect: after execution → refresh properties namespace
            QObject::connect(view, &PythonViewWidget::executionFinished,
                             props, &PythonPropertiesWidget::refreshNamespace);

            // Connect: insert code from properties → console input
            QObject::connect(props, &PythonPropertiesWidget::insertCodeRequested,
                             view, [view](QString const & code) {
                                 if (auto * console = view->consoleWidget()) {
                                     auto * input = console->inputEdit();
                                     if (input) {
                                         input->insertPlainText(code);
                                     }
                                 }
                             });

            // Register the state
            reg->registerState(state);

            return EditorRegistry::EditorInstance{
                .state = state,
                .view = view,
                .properties = props
            };
        }
    });
}

}  // namespace PythonWidgetModule
