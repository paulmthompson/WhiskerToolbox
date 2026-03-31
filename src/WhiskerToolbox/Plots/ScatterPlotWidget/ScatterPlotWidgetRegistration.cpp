#include "ScatterPlotWidgetRegistration.hpp"

#include "Core/ScatterAxisSource.hpp"
#include "Core/ScatterPlotState.hpp"
#include "Rendering/ScatterPlotOpenGLWidget.hpp"
#include "UI/ScatterPlotPropertiesWidget.hpp"
#include "UI/ScatterPlotWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/ContextAction.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/SelectionContext.hpp"
#include "GroupManagementWidget/GroupManager.hpp"
#include "KeymapSystem/KeyAction.hpp"
#include "KeymapSystem/KeymapManager.hpp"
#include "Tensors/TensorData.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>
#include <utility>

namespace ScatterPlotWidgetModule {

namespace {

void registerPolygonActions(KeymapSystem::KeymapManager * km) {
    if (!km) {
        return;
    }
    km->registerAction({.action_id = QStringLiteral("scatter_plot.polygon_complete"),
                        .display_name = QStringLiteral("Complete Polygon"),
                        .category = QStringLiteral("Scatter Plot"),
                        .scope = KeymapSystem::KeyActionScope::editorFocused(
                                EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget"))),
                        .default_binding = QKeySequence(Qt::Key_Return)});

    km->registerAction({.action_id = QStringLiteral("scatter_plot.polygon_cancel"),
                        .display_name = QStringLiteral("Cancel Polygon"),
                        .category = QStringLiteral("Scatter Plot"),
                        .scope = KeymapSystem::KeyActionScope::editorFocused(
                                EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget"))),
                        .default_binding = QKeySequence(Qt::Key_Escape)});

    km->registerAction({.action_id = QStringLiteral("scatter_plot.polygon_undo_vertex"),
                        .display_name = QStringLiteral("Undo Last Vertex"),
                        .category = QStringLiteral("Scatter Plot"),
                        .scope = KeymapSystem::KeyActionScope::editorFocused(
                                EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget"))),
                        .default_binding = QKeySequence(Qt::Key_Backspace)});
}

void registerContextActions(EditorRegistry * registry,
                            std::shared_ptr<DataManager> const & dm) {
    auto * selection_ctx = registry->selectionContext();
    if (!selection_ctx) {
        return;
    }

    ContextAction visualize_action;
    visualize_action.action_id = QStringLiteral("scatter_plot.visualize_2d_tensor");
    visualize_action.display_name = QStringLiteral("Visualize in Scatter Plot");
    visualize_action.producer_type = EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget"));

    visualize_action.is_applicable = [dm](SelectionContext const & ctx) -> bool {
        if (ctx.dataFocusType() != QStringLiteral("TensorData")) {
            return false;
        }
        auto const key = ctx.dataFocus().toStdString();
        auto tensor = dm->getData<TensorData>(key);
        return tensor && tensor->numColumns() >= 2;
    };

    visualize_action.execute = [registry](SelectionContext const & ctx) {
        auto state = registry->openEditor(EditorLib::EditorTypeId(QStringLiteral("ScatterPlotWidget")));
        if (!state) {
            return;
        }
        auto scatter_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
        if (!scatter_state) {
            return;
        }

        auto const key = ctx.dataFocus().toStdString();

        ScatterAxisSource x_source;
        x_source.data_key = key;
        x_source.tensor_column_index = 0;

        ScatterAxisSource y_source;
        y_source.data_key = key;
        y_source.tensor_column_index = 1;

        scatter_state->setXSource(x_source);
        scatter_state->setYSource(y_source);
    };

    selection_ctx->registerAction(std::move(visualize_action));
}

}// namespace

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> const & data_manager,
                   GroupManager * group_manager,
                   KeymapSystem::KeymapManager * keymap_manager) {

    if (!registry) {
        std::cerr << "ScatterPlotWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto const & dm = data_manager;
    auto gm = group_manager;
    auto km = keymap_manager;

    registerPolygonActions(km);

    registry->registerType({.type_id = QStringLiteral("ScatterPlotWidget"),
                            .display_name = QStringLiteral("Scatter Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Scatter Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<ScatterPlotState>(); },

                            // View factory - creates ScatterPlotWidget (the view component)
                            .create_view = [dm, registry, gm, km](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "ScatterPlotWidgetModule: Failed to cast state to ScatterPlotState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new ScatterPlotWidget(dm);
                                widget->setState(plot_state);

                                if (gm) {
                                    widget->setGroupManager(gm);
                                }
                                widget->setKeymapManager(km);

                                return widget;
                            },

                            // Properties factory - creates ScatterPlotPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<ScatterPlotState>(state);
                                if (!plot_state) {
                                    std::cerr << "ScatterPlotWidgetModule: Failed to cast state to ScatterPlotState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new ScatterPlotPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm, gm, km](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<ScatterPlotState>();

                                // Create the view widget
                                auto * view = new ScatterPlotWidget(dm);
                                view->setState(state);

                                if (gm) {
                                    view->setGroupManager(gm);
                                }
                                view->setKeymapManager(km);

                                // Set selection context for ContextAction menu integration
                                if (reg->selectionContext()) {
                                    view->setSelectionContext(reg->selectionContext());
                                }

                                // Create the properties widget with the shared state
                                auto * props = new ScatterPlotPropertiesWidget(state, dm);
                                props->setPlotWidget(view);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the scatter plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &ScatterPlotWidget::timePositionSelected,
                                                     [reg](TimePosition const & position) {
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

    // Register "Visualize in Scatter Plot" ContextAction
    registerContextActions(registry, dm);
}

}// namespace ScatterPlotWidgetModule
