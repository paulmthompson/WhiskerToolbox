#include "DataInspectorWidgetRegistration.hpp"

#include "DataInspectorPropertiesWidget.hpp"
#include "DataInspectorState.hpp"
#include "DataInspectorViewWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/OperationContext.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"

#include <QKeySequence>

#include <iostream>
#include <utility>

namespace DataInspectorModule {

namespace {

/// @brief Register the three DigitalIntervalSeries keymap actions (no default keys)
void registerIntervalActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }

    auto const scope = KeymapSystem::KeyActionScope::alwaysRouted(
            EditorLib::EditorTypeId(QStringLiteral("DigitalIntervalSeriesInspector")));
    QString const category = QStringLiteral("Digital Interval Series");

    km->registerAction({.action_id = QStringLiteral("digital_interval_series.mark_contact_start"),
                        .display_name = QStringLiteral("Mark Contact Start"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence()});

    km->registerAction({.action_id = QStringLiteral("digital_interval_series.mark_contact_end"),
                        .display_name = QStringLiteral("Mark Contact End"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence()});

    km->registerAction({.action_id = QStringLiteral("digital_interval_series.flip_current_frame"),
                        .display_name = QStringLiteral("Flip Current Frame"),
                        .category = category,
                        .scope = scope,
                        .default_binding = QKeySequence()});
}

}// namespace

void registerTypes(EditorRegistry * registry,
                   const std::shared_ptr<DataManager>& data_manager,
                   GroupManager * group_manager,
                   commands::CommandRecorder * recorder,
                   KeymapSystem::KeymapManager * keymap_manager) {

    if (!registry) {
        std::cerr << "DataInspectorModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Register configurable keyboard shortcuts for interval marking
    registerIntervalActions(keymap_manager);

    // Capture dependencies for lambdas
    auto const & dm = std::move(data_manager);
    auto gm = group_manager;
    auto cr = recorder;
    auto * km = keymap_manager;

    registry->registerType({.type_id = QStringLiteral("DataInspector"),
                            .display_name = QStringLiteral("Data Inspector"),
                            .icon_path = QStringLiteral(":/icons/inspect.png"),
                            .menu_path = QStringLiteral("View/Data"),
                            .preferred_zone = Zone::Center,// View widget goes to center
                            .properties_zone = Zone::Right,// Properties widget goes to right
                            .prefers_split = false,
                            .properties_as_tab = true,     // Add as tab, don't replace
                            .auto_raise_properties = false,// Don't obscure other tools
                            .allow_multiple = true,        // Multiple inspectors allowed

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<DataInspectorState>(); },

                            // View factory - creates DataInspectorViewWidget (Center zone)
                            .create_view = [dm](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto inspector_state = std::dynamic_pointer_cast<DataInspectorState>(state);
                                if (!inspector_state) {
                                    std::cerr << "DataInspectorModule: Failed to cast state to DataInspectorState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new DataInspectorViewWidget(dm);
                                widget->setState(inspector_state);
                                return widget;
                            },

                            // Properties factory - creates DataInspectorPropertiesWidget (Right zone)
                            .create_properties = [dm, gm, cr, km](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto inspector_state = std::dynamic_pointer_cast<DataInspectorState>(state);
                                if (!inspector_state) {
                                    std::cerr << "DataInspectorModule: Failed to cast state to DataInspectorState for properties" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new DataInspectorPropertiesWidget(dm, gm);
                                widget->setState(inspector_state);
                                widget->setCommandRecorder(cr);
                                widget->setKeymapManager(km);
                                return widget;
                            },

                            // Custom editor creation for complex view/properties coupling
                            // Ensures both widgets share the same state and SelectionContext
                            .create_editor_custom = [dm, gm, cr, km](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<DataInspectorState>();

                                // Create the view widget (Center zone)
                                auto * view = new DataInspectorViewWidget(dm);
                                view->setState(state);

                                // Create the properties widget (Right zone)
                                auto * props = new DataInspectorPropertiesWidget(dm, gm);
                                props->setState(state);
                                props->setCommandRecorder(cr);
                                props->setKeymapManager(km);

                                // Connect properties to selection context from registry
                                if (reg) {
                                    props->setSelectionContext(reg->selectionContext());
                                    props->setOperationContext(reg->operationContext());
                                }

                                // Connect frame selection signals
                                QObject::connect(props, &DataInspectorPropertiesWidget::frameSelected,
                                                 view, &DataInspectorViewWidget::frameSelected);

                                // Connect view widget frame selection to update time in both DataManager and EditorRegistry
                                // This allows double-clicking on table cells to navigate to the corresponding frame
                                if (reg && dm) {
                                    QObject::connect(view, &DataInspectorViewWidget::frameSelected,
                                                     [reg](const TimePosition& position) {
                                                         // Update EditorRegistry time (triggers timeChanged signal for other widgets)
                                                         reg->setCurrentTime(position);
                                                     });
                                }

                                // Connect properties widget to view widget for inspector-view communication
                                props->setViewWidget(view);

                                // Register the state
                                reg->registerState(state);

                                // Initialize current_position from EditorRegistry's current position
                                if (reg) {
                                    state->current_position = reg->currentPosition();

                                    // Connect EditorRegistry time changes to update state's current_position
                                    // This allows sub-widgets to access the current time position from the state
                                    QObject::connect(reg,
                                                     QOverload<TimePosition>::of(&EditorRegistry::timeChanged),
                                                     [state](TimePosition position) {
                                                         state->current_position = std::move(position);
                                                     });
                                }

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = view,
                                        .properties = props};
                            }});
}

}// namespace DataInspectorModule
