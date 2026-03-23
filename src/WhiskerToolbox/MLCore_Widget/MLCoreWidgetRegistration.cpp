#include "MLCoreWidgetRegistration.hpp"

#include "Core/MLCoreWidgetState.hpp"
#include "DataManager/DataManager.hpp"
#include "EditorState/ContextAction.hpp"
#include "EditorState/EditorRegistry.hpp"
#include "EditorState/SelectionContext.hpp"
#include "MLCoreWidget.hpp"
#include "Tensors/TensorData.hpp"

#include <iostream>

namespace MLCoreWidgetModule {

void registerTypes(EditorRegistry * registry,
                   std::shared_ptr<DataManager> data_manager,
                   GroupManager * group_manager) {

    if (!registry) {
        std::cerr << "MLCoreWidgetModule::registerTypes: registry is null" << std::endl;
        return;
    }

    auto dm = std::move(data_manager);

    registry->registerType({.type_id = QStringLiteral("MLCoreWidget"),
                            .display_name = QStringLiteral("ML Workflow"),
                            .icon_path = QString{},
                            .menu_path = QStringLiteral("View/Analysis"),

                            // Zone placement: single-instance tool panel in the right zone
                            .preferred_zone = Zone::Right,
                            .properties_zone = Zone::Right,
                            .prefers_split = false,
                            .properties_as_tab = true,
                            .auto_raise_properties = false,

                            .allow_multiple = false,

                            .create_state = []() { return std::make_shared<MLCoreWidgetState>(); },

                            // Use create_editor_custom for EditorRegistry access (SelectionContext)
                            .create_view = nullptr,
                            .create_properties = nullptr,

                            .create_editor_custom = [dm, group_manager](EditorRegistry * reg)
                                    -> EditorRegistry::EditorInstance {
                                auto state = std::make_shared<MLCoreWidgetState>();

                                auto * selection_context = reg->selectionContext();

                                auto * widget = new MLCoreWidget(state, dm, selection_context,
                                                                 group_manager, nullptr);

                                widget->setMinimumSize(350, 500);
                                widget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);

                                reg->registerState(state);

                                return EditorRegistry::EditorInstance{
                                        .state = state,
                                        .view = widget,
                                        .properties = nullptr};
                            }});

    // Register "Cluster with K-Means" ContextAction
    auto * selection_ctx = registry->selectionContext();
    if (selection_ctx) {
        ContextAction cluster_action;
        cluster_action.action_id = QStringLiteral("mlcore.cluster_tensor");
        cluster_action.display_name = QStringLiteral("Cluster with K-Means");
        cluster_action.producer_type = EditorLib::EditorTypeId(QStringLiteral("MLCoreWidget"));

        cluster_action.is_applicable = [dm](SelectionContext const & ctx) -> bool {
            if (ctx.dataFocusType() != QStringLiteral("TensorData")) {
                return false;
            }
            auto const key = ctx.dataFocus().toStdString();
            auto tensor = dm->getData<TensorData>(key);
            return tensor != nullptr;
        };

        cluster_action.execute = [registry](SelectionContext const & ctx) {
            auto state = registry->openEditor(EditorLib::EditorTypeId(QStringLiteral("MLCoreWidget")));
            if (!state) {
                return;
            }
            auto ml_state = std::dynamic_pointer_cast<MLCoreWidgetState>(state);
            if (!ml_state) {
                return;
            }

            ml_state->setClusteringTensorKey(ctx.dataFocus().toStdString());
            ml_state->setActiveTab(1);// Switch to Clustering tab
        };

        selection_ctx->registerAction(std::move(cluster_action));
    }
}

}// namespace MLCoreWidgetModule
