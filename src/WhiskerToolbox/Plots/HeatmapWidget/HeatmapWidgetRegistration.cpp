#include "HeatmapWidgetRegistration.hpp"

#include "Core/HeatmapState.hpp"
#include "UI/HeatmapPropertiesWidget.hpp"
#include "UI/HeatmapWidget.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/ContextAction.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/SelectionContext.hpp"
#include "Plots/EventPlotWidget/Core/EventPlotState.hpp"
#include "Plots/PSTHWidget/Core/PSTHState.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <iostream>
#include <utility>

namespace HeatmapWidgetModule {

namespace {

/**
 * @brief Register ContextActions for opening heatmap units in other plot widgets
 *
 * Registers "Open in Event Plot" and "Open in PSTH" actions that become
 * applicable when the data focus is a DigitalEventSeries. The heatmap's
 * alignment, estimation, and scaling settings are transferred to the
 * newly opened widget.
 */
void registerContextActions(EditorRegistry * registry,
                            std::shared_ptr<HeatmapState> const & heatmap_state) {
    auto * selection_ctx = registry->selectionContext();
    if (!selection_ctx) {
        return;
    }

    // --- Open in Event Plot ---
    ContextAction event_plot_action;
    event_plot_action.action_id = QStringLiteral("heatmap.open_in_event_plot");
    event_plot_action.display_name = QStringLiteral("Open in Event Plot");
    event_plot_action.producer_type = EditorLib::EditorTypeId(QStringLiteral("HeatmapWidget"));

    event_plot_action.is_applicable = [](SelectionContext const & ctx) -> bool {
        return ctx.dataFocusType() == QStringLiteral("DigitalEventSeries");
    };

    event_plot_action.execute = [registry, heatmap_state](SelectionContext const & ctx) {
        auto base_state = registry->openEditor(
                EditorLib::EditorTypeId(QStringLiteral("EventPlotWidget")));
        if (!base_state) {
            return;
        }

        auto event_state = std::dynamic_pointer_cast<EventPlotState>(base_state);
        if (!event_state) {
            return;
        }

        // Transfer alignment settings from the heatmap
        event_state->setAlignmentEventKey(heatmap_state->getAlignmentEventKey());
        event_state->setIntervalAlignmentType(heatmap_state->getIntervalAlignmentType());
        event_state->setOffset(heatmap_state->getOffset());
        event_state->setWindowSize(heatmap_state->getWindowSize());

        auto const & unit_key = ctx.dataFocus().toString();
        event_state->addPlotEvent(unit_key, unit_key);
    };

    selection_ctx->registerAction(std::move(event_plot_action));

    // --- Open in PSTH ---
    ContextAction psth_action;
    psth_action.action_id = QStringLiteral("heatmap.open_in_psth");
    psth_action.display_name = QStringLiteral("Open in PSTH");
    psth_action.producer_type = EditorLib::EditorTypeId(QStringLiteral("HeatmapWidget"));

    psth_action.is_applicable = [](SelectionContext const & ctx) -> bool {
        return ctx.dataFocusType() == QStringLiteral("DigitalEventSeries");
    };

    psth_action.execute = [registry, heatmap_state](SelectionContext const & ctx) {
        auto base_state = registry->openEditor(
                EditorLib::EditorTypeId(QStringLiteral("PSTHWidget")));
        if (!base_state) {
            return;
        }

        auto psth_state = std::dynamic_pointer_cast<PSTHState>(base_state);
        if (!psth_state) {
            return;
        }

        // Transfer alignment, estimation, and scaling settings from the heatmap
        psth_state->setAlignmentEventKey(heatmap_state->getAlignmentEventKey());
        psth_state->setIntervalAlignmentType(heatmap_state->getIntervalAlignmentType());
        psth_state->setOffset(heatmap_state->getOffset());
        psth_state->setWindowSize(heatmap_state->getWindowSize());
        psth_state->setEstimationParams(heatmap_state->estimationParams());
        psth_state->setScaling(heatmap_state->scaling());

        auto const & unit_key = ctx.dataFocus().toString();
        psth_state->addPlotEvent(unit_key, unit_key);
    };

    selection_ctx->registerAction(std::move(psth_action));
}

}// namespace

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> const & data_manager) {

    if (!registry) {
        std::cerr << "HeatmapWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    // Capture dependencies for lambdas
    auto const & dm = data_manager;
    auto reg = registry;

    registry->registerType({.type_id = QStringLiteral("HeatmapWidget"),
                            .display_name = QStringLiteral("Heatmap Plot"),
                            .icon_path = QStringLiteral(""),// No icon for now
                            .menu_path = QStringLiteral("Plot/Heatmap Plot"),
                            .preferred_zone = Zone::Center,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,
                            .allow_multiple = true,

                            // State factory - creates the shared state object
                            .create_state = []() { return std::make_shared<HeatmapState>(); },

                            // View factory - creates HeatmapWidget (the view component)
                            .create_view = [dm, reg](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<HeatmapState>(state);
                                if (!plot_state) {
                                    std::cerr << "HeatmapWidgetModule: Failed to cast state to HeatmapState" << std::endl;
                                    return nullptr;
                                }

                                auto * widget = new HeatmapWidget(dm);
                                widget->setState(plot_state);

                                return widget;
                            },

                            // Properties factory - creates HeatmapPropertiesWidget
                            .create_properties = [dm](std::shared_ptr<EditorState> const & state) -> QWidget * {
                                auto plot_state = std::dynamic_pointer_cast<HeatmapState>(state);
                                if (!plot_state) {
                                    std::cerr << "HeatmapWidgetModule: Failed to cast state to HeatmapState for properties" << std::endl;
                                    return nullptr;
                                }

                                // Create properties widget with shared state
                                auto * props = new HeatmapPropertiesWidget(plot_state, dm);
                                return props;
                            },

                            // Custom editor creation for potential future view/properties coupling
                            .create_editor_custom = [dm](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                // Create the shared state
                                auto state = std::make_shared<HeatmapState>();

                                // Create the view widget
                                auto * view = new HeatmapWidget(dm);
                                view->setState(state);

                                // Wire SelectionContext for dynamic ContextAction menus
                                if (reg->selectionContext()) {
                                    view->setSelectionContext(reg->selectionContext());
                                }

                                // Register ContextActions for opening units in other plots
                                registerContextActions(reg, state);

                                // Create the properties widget with the shared state
                                auto * props = new HeatmapPropertiesWidget(state, dm);
                                props->setPlotWidget(view);

                                QObject::connect(props, &HeatmapPropertiesWidget::exportSVGRequested,
                                                 view, &HeatmapWidget::handleExportSVG);

                                // Connect view widget time position selection to update time in EditorRegistry
                                // This allows the heatmap plot to navigate to a specific time position
                                if (reg) {
                                    QObject::connect(view, &HeatmapWidget::timePositionSelected,
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
}

}// namespace HeatmapWidgetModule
